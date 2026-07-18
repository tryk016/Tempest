#if defined(TEMPEST_BUILD_METAL)

#include "metalapi.h"

#if __has_feature(objc_arc)
#error "Objective C++ ARC is not supported"
#endif

#include <Tempest/Log>
#include <Tempest/Pixmap>
#include <Tempest/Device>
#include <Tempest/Encoder>
#include <Tempest/StorageBuffer>
#include <Tempest/Texture2d>

#include "gapi/metal/mtdevice.h"
#include "gapi/metal/mtbuffer.h"
#include "gapi/metal/mtshader.h"
#include "gapi/metal/mtpipeline.h"
#include "gapi/metal/mtcommandbuffer.h"
#include "gapi/metal/mttexture.h"
#include "gapi/metal/mtpipelinelay.h"
#include "gapi/metal/mtdescriptorarray.h"
#include "gapi/metal/mtsync.h"
#include "gapi/metal/mtswapchain.h"
#include "gapi/metal/mtaccelerationstructure.h"

#include <Metal/Metal.hpp>

using namespace Tempest;
using namespace Tempest::Detail;

MetalApi::MetalApi(ApiFlags f) {
  if((f & ApiFlags::Validation)==ApiFlags::Validation) {
    setenv("METAL_DEVICE_WRAPPER_TYPE","1",1);
    setenv("METAL_DEBUG_ERROR_MODE",   "5",0);
    setenv("METAL_ERROR_MODE",         "5",0);
    validation = true;
    }
  }

MetalApi::~MetalApi() {
  }

BorrowedMetalDevice MetalApi::borrowDevice(const Tempest::Device& device) noexcept {
  auto* nativeDevice = dynamic_cast<MtDevice*>(device.dev);
  if(nativeDevice==nullptr || nativeDevice->impl==nullptr)
    return {};
  return BorrowedMetalDevice(nativeDevice->impl.get());
  }

BorrowedMetalBuffer MetalApi::borrowBuffer(const Tempest::Device& device,
                                           const StorageBuffer& buffer) noexcept {
  auto* nativeDevice = dynamic_cast<MtDevice*>(device.dev);
  if(nativeDevice==nullptr)
    return {};

  auto* nativeBuffer = dynamic_cast<MtBuffer*>(buffer.impl.impl.handler);
  if(nativeBuffer==nullptr || nativeBuffer->dev!=nativeDevice || nativeBuffer->impl==nullptr)
    return {};
  return BorrowedMetalBuffer(nativeBuffer->impl.get());
  }

BorrowedMetalTexture MetalApi::borrowTexture(const Tempest::Device& device,
                                             const Texture2d& texture) noexcept {
  auto* nativeDevice = dynamic_cast<MtDevice*>(device.dev);
  if(nativeDevice==nullptr)
    return {};

  auto* nativeTexture = dynamic_cast<MtTexture*>(texture.impl.handler);
  if(nativeTexture==nullptr || &nativeTexture->dev!=nativeDevice || nativeTexture->impl==nullptr)
    return {};
  return BorrowedMetalTexture(nativeTexture->impl.get());
  }

bool MetalApi::withActiveRenderEncoder(
    const Tempest::Device& device,
    Tempest::Encoder<Tempest::CommandBuffer>& encoder,
    void* context,
    MetalRenderEncodeCallback callback) {
  auto* nativeDevice  = dynamic_cast<MtDevice*>(device.dev);
  auto* nativeCommand = dynamic_cast<MtCommandBuffer*>(encoder.impl);
  if(nativeDevice==nullptr || nativeCommand==nullptr ||
     &nativeCommand->device!=nativeDevice ||
     nativeCommand->encDraw==nullptr || callback==nullptr)
    return false;

  const auto invalidateState = [&]() noexcept {
    encoder.state.curPipeline = nullptr;
    encoder.state.curCompute  = nullptr;
    nativeCommand->curDrawPipeline = nullptr;
    nativeCommand->curCompPipeline = nullptr;
    nativeCommand->curLay           = nullptr;
    nativeCommand->bindings.durty   = true;
    nativeCommand->pushData.durty   = true;
    };

  invalidateState();
  try {
    callback(context,nativeCommand->encDraw.get());
    }
  catch(...) {
    invalidateState();
    throw;
    }
  invalidateState();
  return true;
  }

std::vector<AbstractGraphicsApi::Props> MetalApi::devices() const {
#if defined(__OSX__)
  auto dev = MTL::CopyAllDevices();
  try {
    std::vector<AbstractGraphicsApi::Props> p(dev->count());
    for(size_t i=0; i<p.size(); ++i) {
      MtDevice::deductProps(p[i],*reinterpret_cast<MTL::Device*>(dev->object(i)));
      }
    dev->release();
    return p;
    }
  catch(...) {
    dev->release();
    throw;
    }
#else
  std::vector<AbstractGraphicsApi::Props> p(1);
  auto     dev    = NsPtr<MTL::Device>(MTL::CreateSystemDefaultDevice());
  uint32_t mslVer = 0;
  MtDevice::deductProps(p[0],*dev);
  return p;
#endif
  }

AbstractGraphicsApi::Device* MetalApi::createDevice(std::string_view gpuName) {
  return new MtDevice(gpuName,validation);
  }

AbstractGraphicsApi::Swapchain *MetalApi::createSwapchain(SystemApi::Window *w,
                                                          AbstractGraphicsApi::Device* d) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtSwapchain(dev,w);
  }

AbstractGraphicsApi::PPipeline MetalApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                        const RenderState &st,
                                                        Topology tp,
                                                        const AbstractGraphicsApi::Shader*const* sh,
                                                        size_t cnt) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  const Detail::MtShader* shader[5] = {};
  for(size_t i=0; i<cnt; ++i)
    shader[i] = reinterpret_cast<const Detail::MtShader*>(sh[i]);
  return PPipeline(new MtPipeline(dx,tp,st,shader,cnt));
  }

AbstractGraphicsApi::PCompPipeline MetalApi::createComputePipeline(AbstractGraphicsApi::Device *d,
                                                                   AbstractGraphicsApi::Shader *cs) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  auto& cx = *reinterpret_cast<const MtShader*>(cs);
  return PCompPipeline(new MtCompPipeline(dx,cx));
  }

AbstractGraphicsApi::PShader MetalApi::createShader(AbstractGraphicsApi::Device *d, const void *source, size_t src_size) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return PShader(new MtShader(dx,source,src_size));
  }

AbstractGraphicsApi::PBuffer MetalApi::createBuffer(AbstractGraphicsApi::Device *d, const void *mem, size_t size,
                                                    MemUsage usage, BufferHeap flg) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);

  MTL::ResourceOptions opt = 0;
  // https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/ResourceOptions.html#//apple_ref/doc/uid/TP40016642-CH17-SW1
  // https://developer.apple.com/documentation/metal/choosing-a-resource-storage-mode-for-intel-and-amd-gpus?language=objc
  switch(flg) {
    case BufferHeap::Device:
      opt |= MTL::ResourceStorageModePrivate;
      break;
    case BufferHeap::Upload: {
      if(dx.impl->hasUnifiedMemory()) {
        // Shared resources are only available on systems with integrated graphics,
        // such as Apple silicon and integrated GPUs on Intel-based Mac computers
        opt |= MTL::ResourceStorageModeShared;
        } else {
        opt |= MTL::ResourceStorageModeManaged;
        }
      opt |= MTL::ResourceCPUCacheModeWriteCombined;
      break;
      }
    case BufferHeap::Readback:
      opt |= MTL::ResourceStorageModeManaged;
      opt |= MTL::ResourceCPUCacheModeDefaultCache;
      break;
    }

  return PBuffer(new MtBuffer(dx,mem,size,opt));
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const Pixmap &p, TextureFormat frm, uint32_t mips) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,p,mips,frm));
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,0,mips,frm,false));
  }

AbstractGraphicsApi::PTexture MetalApi::createStorage(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,0,mips,frm,true));
  }

AbstractGraphicsApi::PTexture MetalApi::createStorage(Device* d,
                                                      const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mips,
                                                      TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,depth,mips,frm,true));
  }

AbstractGraphicsApi::AccelerationStructure* MetalApi::createBottomAccelerationStruct(Device* d, const RtGeometry* geom, size_t size) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  // auto& ix  = *reinterpret_cast<MtBuffer*>(ibo);
  // auto& vx  = *reinterpret_cast<MtBuffer*>(vbo);
  return new MtAccelerationStructure(dev, geom, size);
  }

AbstractGraphicsApi::AccelerationStructure* MetalApi::createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t size) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtTopAccelerationStructure(dev,inst,as,size);
  }

AbstractGraphicsApi::DescArray* MetalApi::createDescriptors(Device* d, Texture** tex, size_t cnt, uint32_t mipLevel) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtDescriptorArray(dev,tex,cnt,mipLevel);
  }

AbstractGraphicsApi::DescArray* MetalApi::createDescriptors(Device* d, Texture** tex, size_t cnt, uint32_t mipLevel,
                                                            const Sampler& smp) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtDescriptorArray(dev,tex,cnt,mipLevel,smp);
  }

AbstractGraphicsApi::DescArray* MetalApi::createDescriptors(Device* d, Buffer** buf, size_t cnt) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtDescriptorArray(dev,buf,cnt);
  }

void MetalApi::readPixels(AbstractGraphicsApi::Device*,
                          Pixmap& out, const AbstractGraphicsApi::PTexture t,
                          TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) {
  auto& tx = *reinterpret_cast<MtTexture*>(t.handler);
  tx.readPixels(out,frm,w,h,mip);
  }

void MetalApi::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer *buf,
                         void *out, size_t size) {
  buf->read(out,0,size);
  }

AbstractGraphicsApi::CommandBuffer *MetalApi::createCommandBuffer(AbstractGraphicsApi::Device *d) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return new MtCommandBuffer(dx);
  }

void MetalApi::present(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Swapchain *sw) {
  auto& s   = *reinterpret_cast<MtSwapchain*>(sw);
  s.present();
  }

std::shared_ptr<AbstractGraphicsApi::Fence> MetalApi::submit(Device* d, CommandBuffer* c) {
  auto* dx = reinterpret_cast<MtDevice*>(d);
  auto& cx = *reinterpret_cast<MtCommandBuffer*>(c);

  if(cx.isRecording())
    throw ConcurentRecordingException();

  MTL::CommandBuffer& cmd = *cx.impl;
  const auto initialStatus = cmd.status();
  if(initialStatus!=MTL::CommandBufferStatusNotEnqueued &&
     initialStatus!=MTL::CommandBufferStatusEnqueued)
    throw DeviceLostException("Metal command buffer cannot be committed in its current state");

  auto pfence = dx->aquireFence();
  if(pfence==nullptr)
    throw DeviceLostException();

  auto async = dx->asyncState();
  MtAsyncState::SubmissionToken token;
  try {
    token = async->onSubmit();
    if(!token)
      throw DeviceLostException(
        "Metal submission rejected after an asynchronous present failure");
    cmd.addCompletedHandler(^(MTL::CommandBuffer* c){
      if(!async->beginCompletion(token))
        return;
      try {
        const MTL::CommandBufferStatus s = c->status();
        NS::Error* const error = c->error();
        const auto errorCode = error!=nullptr ?
          MTL::CommandBufferError(error->code()) : MTL::CommandBufferErrorNone;
        dx->signalFence(*pfence,s,errorCode,error);
        }
      catch(...) {
        dx->signalFence(*pfence,MTL::CommandBufferStatusError,
                        MTL::CommandBufferErrorInternal,nullptr);
        }
      async->finishCompletion(token);
      });
    cmd.commit();
    }
  catch(...) {
    if(!token || async->beginCompletion(token)) {
      dx->signalFence(*pfence,MTL::CommandBufferStatusError,
                      MTL::CommandBufferErrorInternal,nullptr);
      if(token)
        async->finishCompletion(token);
      }
    throw;
    }
  return pfence;
  }

void MetalApi::getCaps(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Props &caps) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  caps = dx.prop;
  }

#endif
