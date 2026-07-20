#if defined(TEMPEST_BUILD_METAL)

#include "mtpipelinearchive.h"

#if __has_feature(objc_arc)
#error "Objective C++ ARC is not supported"
#endif

#include "mtdevice.h"
#include "nsptr.h"

#include <Metal/Metal.hpp>

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace Tempest;
using namespace Tempest::Detail;

namespace {

std::atomic<uint64_t> temporaryArchiveId = 0;

NsPtr<NS::URL> fileUrl(const std::string& path) {
  auto string = NsPtr<NS::String>(
      NS::String::alloc()->init(path.c_str(),NS::UTF8StringEncoding));
  if(string==nullptr)
    return {};
  return NsPtr<NS::URL>(
      NS::URL::alloc()->initFileURLWithPath(string.get()));
  }

NsPtr<NS::Array> binaryArchiveArray(MTL::BinaryArchive& archive) {
  const NS::Object* object = &archive;
  return NsPtr<NS::Array>(
      NS::Array::alloc()->init(&object,1));
  }

}

struct MtPipelineArchive::Impl final {
  explicit Impl(
      MTL::Device& device,
      std::shared_ptr<const MetalPipelineArchiveConfigOwned> config)
    :device(device), config(std::move(config)) {
    state.flags |= MetalPipelineArchiveSnapshot::Available;
    if(this->config==nullptr)
      return;
    state.flags |= MetalPipelineArchiveSnapshot::Configured;
    open();
    }

  void open() {
    struct stat fileStatus = {};
    const bool exists =
        ::stat(config->archivePath.c_str(),&fileStatus)==0;
    if(!exists && errno!=ENOENT) {
      ++state.loadFailures;
      disable();
      return;
      }

    if(exists) {
      auto url = fileUrl(config->archivePath);
      if(url!=nullptr) {
        auto descriptor = NsPtr<MTL::BinaryArchiveDescriptor>::init();
        descriptor->setUrl(url.get());
        NS::Error* error = nullptr;
        archive = NsPtr<MTL::BinaryArchive>(
            device.newBinaryArchive(descriptor.get(),&error));
        }
      if(archive!=nullptr) {
        state.flags |= MetalPipelineArchiveSnapshot::LoadedFromDisk;
        return;
        }
      ++state.loadFailures;
      ++state.rebuilds;
      }

    auto descriptor = NsPtr<MTL::BinaryArchiveDescriptor>::init();
    NS::Error* error = nullptr;
    archive = NsPtr<MTL::BinaryArchive>(
        device.newBinaryArchive(descriptor.get(),&error));
    if(archive==nullptr) {
      disable();
      return;
      }
    state.flags |= MetalPipelineArchiveSnapshot::CreatedEmpty;
    }

  void disable() noexcept {
    state.flags |= MetalPipelineArchiveSnapshot::DisabledAfterError;
    archive = nullptr;
    }

  MTL::RenderPipelineState* create(
      MtDevice& owner,
      MTL::RenderPipelineDescriptor& descriptor,
      MTL::PipelineOption options,
      NS::Error** outputError) {
    owner.noteRenderPsoRequest();
    NS::Error* error = nullptr;
    auto* result = device.newRenderPipelineState(
        &descriptor,options,nullptr,&error);
    if(outputError!=nullptr)
      *outputError = error;
    return result;
    }

  MTL::RenderPipelineState* fallback(
      MtDevice& owner,
      MTL::RenderPipelineDescriptor& descriptor,
      NS::Error** outputError,
      bool disableOnSuccess) {
    descriptor.setBinaryArchives(nullptr);
    auto* result = create(
        owner,descriptor,MTL::PipelineOptionNone,outputError);
    if(result!=nullptr) {
      ++state.renderFallbacks;
      if(disableOnSuccess)
        disable();
      }
    return result;
    }

  MTL::RenderPipelineState* createArchived(
      MtDevice& owner,
      MTL::RenderPipelineDescriptor& descriptor,
      NS::Error** outputError) {
    const bool loaded =
        (state.flags&
         MetalPipelineArchiveSnapshot::LoadedFromDisk)!=0;
    descriptor.setBinaryArchives(nullptr);

    if(loaded) {
      auto archives = binaryArchiveArray(*archive);
      descriptor.setBinaryArchives(archives.get());
      auto* strict = create(
          owner,descriptor,
          MTL::PipelineOptionFailOnBinaryArchiveMiss,outputError);
      if(strict!=nullptr) {
        ++state.renderHits;
        return strict;
        }
      }

    descriptor.setBinaryArchives(nullptr);
    NS::Error* addError = nullptr;
    const bool added =
        archive->addRenderPipelineFunctions(&descriptor,&addError);
    if(added) {
      ++state.renderAdds;
      state.flags |= MetalPipelineArchiveSnapshot::Dirty;

      auto archives = binaryArchiveArray(*archive);
      descriptor.setBinaryArchives(archives.get());
      auto* strict = create(
          owner,descriptor,
          MTL::PipelineOptionFailOnBinaryArchiveMiss,outputError);
      if(strict!=nullptr) {
        ++state.renderMisses;
        return strict;
        }

      auto* ordinary = fallback(
          owner,descriptor,outputError,true);
      if(ordinary!=nullptr)
        ++state.renderMisses;
      return ordinary;
      }

    auto* ordinary = fallback(
        owner,descriptor,outputError,false);
    if(ordinary!=nullptr) {
      ++state.renderMisses;
      disable();
      }
    return ordinary;
    }

  bool flushLocked() noexcept {
    if(config==nullptr)
      return true;

    ++state.flushAttempts;
    if((state.flags&
        MetalPipelineArchiveSnapshot::DisabledAfterError)!=0 ||
       archive==nullptr) {
      ++state.flushFailures;
      return false;
      }
    if((state.flags&MetalPipelineArchiveSnapshot::Dirty)==0) {
      ++state.flushSuccesses;
      return true;
      }

    try {
      const uint64_t id =
          temporaryArchiveId.fetch_add(1,std::memory_order_relaxed);
      const std::string temporary =
          config->archivePath+".tmp."+
          std::to_string(static_cast<unsigned long long>(::getpid()))+"."+
          std::to_string(static_cast<unsigned long long>(id));
      ::unlink(temporary.c_str());

      auto url = fileUrl(temporary);
      NS::Error* error = nullptr;
      const bool serialized =
          url!=nullptr && archive->serializeToURL(url.get(),&error);
      if(!serialized) {
        ::unlink(temporary.c_str());
        ++state.flushFailures;
        disable();
        return false;
        }
      if(::rename(temporary.c_str(),config->archivePath.c_str())!=0) {
        ::unlink(temporary.c_str());
        ++state.flushFailures;
        disable();
        return false;
        }

      state.flags &= ~MetalPipelineArchiveSnapshot::Dirty;
      ++state.flushSuccesses;
      return true;
      }
    catch(...) {
      ++state.flushFailures;
      disable();
      return false;
      }
    }

  MTL::Device& device;
  const std::shared_ptr<const MetalPipelineArchiveConfigOwned> config;
  NsPtr<MTL::BinaryArchive> archive;
  MetalPipelineArchiveSnapshot state;
  mutable std::mutex sync;
};

MtPipelineArchive::MtPipelineArchive(
    MTL::Device& device,
    std::shared_ptr<const MetalPipelineArchiveConfigOwned> config)
  :impl(std::make_unique<Impl>(device,std::move(config))) {
  }

MtPipelineArchive::~MtPipelineArchive() {
  if(impl==nullptr)
    return;
  std::lock_guard<std::mutex> guard(impl->sync);
  if((impl->state.flags&MetalPipelineArchiveSnapshot::Dirty)!=0)
    (void)impl->flushLocked();
  }

MTL::RenderPipelineState* MtPipelineArchive::newRenderPipelineState(
    MtDevice& device,
    MTL::RenderPipelineDescriptor& descriptor,
    MetalBuiltinRenderRole role,
    NS::Error** error) {
  if(error!=nullptr)
    *error = nullptr;

  if(!isMetalPipelineArchiveRenderRole(role) ||
     impl->config==nullptr) {
    descriptor.setBinaryArchives(nullptr);
    return impl->create(
        device,descriptor,MTL::PipelineOptionNone,error);
    }

  std::lock_guard<std::mutex> guard(impl->sync);
  const bool enabled =
      impl->archive!=nullptr &&
      (impl->state.flags&
       MetalPipelineArchiveSnapshot::DisabledAfterError)==0;
  if(!enabled) {
    descriptor.setBinaryArchives(nullptr);
    return impl->create(
        device,descriptor,MTL::PipelineOptionNone,error);
    }
  return impl->createArchived(device,descriptor,error);
  }

MetalPipelineArchiveSnapshot
MtPipelineArchive::snapshot() const noexcept {
  try {
    std::lock_guard<std::mutex> guard(impl->sync);
    return impl->state;
    }
  catch(...) {
    return {};
    }
  }

bool MtPipelineArchive::flush() noexcept {
  try {
    std::lock_guard<std::mutex> guard(impl->sync);
    return impl->flushLocked();
    }
  catch(...) {
    return false;
    }
  }

#endif
