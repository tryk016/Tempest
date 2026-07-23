#include <Tempest/MetalApi>
#include <Tempest/Builtin>
#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__OSX__)
#include <unistd.h>
#endif

#include "gapi_test_common.h"

#if defined(__OSX__)
#include <Metal/Metal.hpp>
#endif

using namespace testing;
using namespace Tempest;

#if defined(__OSX__)
namespace {

void observeActiveRenderEncoder(void* context,
                                MTL::RenderCommandEncoder* encoder) {
  auto& called = *static_cast<bool*>(context);
  called = encoder!=nullptr;
  }

void throwFromActiveRenderEncoder(void*,
                                  MTL::RenderCommandEncoder*) {
  throw std::runtime_error("active render encoder callback");
  }

struct NativeClearContext final {
  MTL::Texture* target = nullptr;
  size_t        encodedPasses = 0;
};

void observeActiveCommandBuffer(void* context,
                                MTL::CommandBuffer* commandBuffer) {
  auto& called = *static_cast<bool*>(context);
  called = commandBuffer!=nullptr;
  }

bool nullContextCommandBufferCallbackCalled = false;

void observeNullContextCommandBuffer(void*,MTL::CommandBuffer*) {
  nullContextCommandBufferCallbackCalled = true;
  }

void encodeNativeClearPasses(void* context,
                             MTL::CommandBuffer* commandBuffer) {
  auto& clear = *static_cast<NativeClearContext*>(context);
  auto pool = NS::TransferPtr(NS::AutoreleasePool::alloc()->init());
  const std::array<MTL::ClearColor,2> colors = {{
      MTL::ClearColor::Make(1.0,0.0,0.0,1.0),
      MTL::ClearColor::Make(0.0,1.0,0.0,1.0),
  }};

  for(const auto color:colors) {
    auto pass = NS::TransferPtr(MTL::RenderPassDescriptor::alloc()->init());
    auto* attachment = pass->colorAttachments()->object(0);
    attachment->setTexture(clear.target);
    attachment->setLoadAction(MTL::LoadActionClear);
    attachment->setStoreAction(MTL::StoreActionStore);
    attachment->setClearColor(color);

    auto* nativeEncoder = commandBuffer->renderCommandEncoder(pass.get());
    if(nativeEncoder==nullptr)
      return;
    nativeEncoder->endEncoding();
    ++clear.encodedPasses;
    }
  }

MetalBuiltinOfflineManifest testOfflineBuiltinManifest() {
  MetalBuiltinOfflineManifest manifest;
  manifest.metallibPath =
      TEMPEST_TEST_METAL_BUILTIN_OFFLINE_LIBRARY;
  manifest.colorVertexFunction = "tempestOfflineColorVertex";
  manifest.colorFragmentFunction = "tempestOfflineColorFragment";
  manifest.textureVertexFunction = "tempestOfflineTextureVertex";
  manifest.textureFragmentFunction = "tempestOfflineTextureFragment";
  return manifest;
  }

std::vector<uint32_t> readSpirvFile(const char* path) {
  std::ifstream input(path,std::ios::binary|std::ios::ate);
  if(!input)
    throw std::runtime_error(std::string("unable to open ")+path);
  const std::streamoff byteSize = input.tellg();
  if(byteSize<=0 || byteSize%sizeof(uint32_t)!=0)
    throw std::runtime_error(std::string("invalid SPIR-V size in ")+path);
  std::vector<uint32_t> spirv(
      static_cast<size_t>(byteSize)/sizeof(uint32_t));
  input.seekg(0);
  input.read(
      reinterpret_cast<char*>(spirv.data()),
      static_cast<std::streamsize>(byteSize));
  if(!input)
    throw std::runtime_error(std::string("unable to read ")+path);
  return spirv;
  }

struct OfflineInventorySources final {
  std::vector<uint32_t> vertex =
      readSpirvFile(TEMPEST_TEST_METAL_INVENTORY_VERTEX_SPIRV);
  std::vector<uint32_t> fragment =
      readSpirvFile(TEMPEST_TEST_METAL_INVENTORY_FRAGMENT_SPIRV);
};

MetalBuiltinOfflineManifest testOfflineInventoryManifest(
    const OfflineInventorySources& sources) {
  auto manifest = testOfflineBuiltinManifest();
  manifest.inventoryVertexSpirv = sources.vertex.data();
  manifest.inventoryVertexSpirvSize =
      sources.vertex.size()*sizeof(uint32_t);
  manifest.inventoryVertexFunction = "tempestOfflineInventoryVertex";
  manifest.inventoryFragmentSpirv = sources.fragment.data();
  manifest.inventoryFragmentSpirvSize =
      sources.fragment.size()*sizeof(uint32_t);
  manifest.inventoryFragmentFunction = "tempestOfflineInventoryFragment";
  return manifest;
  }

std::string pipelineArchiveTestPath(const char* suffix) {
  return std::string("/tmp/tempest-metal-pipeline-archive-")+
         std::to_string(
             static_cast<unsigned long long>(::getpid()))+
         suffix;
  }

void instantiateArchivedBuiltinPipelines(
    Device& device,
    bool opaque = true,
    bool alpha = true,
    bool colorAlpha = true) {
  const auto& color = device.builtin().empty();
  const auto& texture = device.builtin().texture2d();
  auto target = device.attachment(TextureFormat::RGBA8,4,4);
  auto command = device.commandBuffer();
  auto encoder = command.startEncoding(device);
  encoder.setFramebuffer(
      {{target,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}});
  if(colorAlpha)
    encoder.setPipeline(color.brushB);
  if(opaque)
    encoder.setPipeline(texture.brush);
  if(alpha)
    encoder.setPipeline(texture.brushB);
  }

void instantiateUnarchivedBuiltinPipeline(Device& device) {
  const auto& texture = device.builtin().texture2d();
  auto target = device.attachment(TextureFormat::RGBA8,4,4);
  auto command = device.commandBuffer();
  auto encoder = command.startEncoding(device);
  encoder.setFramebuffer(
      {{target,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}});
  encoder.setPipeline(texture.brushA);
  }

void instantiateArchivedInventoryPipeline(
    Device& device, const OfflineInventorySources& sources) {
  auto vert = device.shader(
      sources.vertex.data(),sources.vertex.size()*sizeof(uint32_t));
  auto frag = device.shader(
      sources.fragment.data(),sources.fragment.size()*sizeof(uint32_t));

  RenderState state;
  state.setCullFaceMode(RenderState::CullMode::Front);
  state.setZTestMode(RenderState::ZTestMode::LEqual);
  state.setZWriteEnabled(true);
  state.setRasterDiscardEnabled(false);
  state.setBlendSource(RenderState::BlendMode::One);
  state.setBlendDest(RenderState::BlendMode::Zero);
  state.setBlendOp(RenderState::BlendOp::Add);
  auto pipeline = device.pipeline(Topology::Triangles,state,vert,frag);

  auto target = device.attachment(TextureFormat::RGBA8,4,4);
  auto depth  = device.zbuffer(TextureFormat::Depth16,4,4);
  auto command = device.commandBuffer();
  auto encoder = command.startEncoding(device);
  encoder.setFramebuffer(
      {{target,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}},
      {depth,1.f,Tempest::Preserve});
  encoder.setPipeline(pipeline);
  }

}
#endif

TEST(MetalApi,MetalApi) {
#if defined(__OSX__)
  GapiTestCommon::init<MetalApi>();
#endif
  }

TEST(MetalApi,Vbo) {
#if defined(__OSX__)
  GapiTestCommon::Vbo<MetalApi>();
#endif
  }

TEST(MetalApi,VboInit) {
#if defined(__OSX__)
  GapiTestCommon::VboInit<MetalApi>();
#endif
  }

TEST(MetalApi,VboDyn) {
#if defined(__OSX__)
  GapiTestCommon::VboDyn<MetalApi>();
#endif
  }

TEST(MetalApi,BorrowedNativeHandles) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device device(api);
    Device foreignDevice(api);

    auto buffer  = device.ssbo(Uninitialized,64);
    auto vbo     = device.vbo(GapiTestCommon::vboData,3);
    auto ibo     = device.ibo(GapiTestCommon::iboData,3);
    auto pixmap  = Pixmap(1,1,TextureFormat::RGBA8);
    auto texture = device.texture(pixmap,false);

    const auto nativeDevice  = MetalApi::borrowDevice(device);
    const auto nativeBuffer  = MetalApi::borrowBuffer(device,buffer);
    const auto nativeVbo     = MetalApi::borrowBuffer(device,vbo);
    const auto nativeIbo     = MetalApi::borrowBuffer(device,ibo);
    const auto nativeTexture = MetalApi::borrowTexture(device,texture);

    EXPECT_TRUE(nativeDevice);
    EXPECT_TRUE(nativeBuffer);
    EXPECT_TRUE(nativeVbo);
    EXPECT_TRUE(nativeIbo);
    EXPECT_TRUE(nativeTexture);
    EXPECT_EQ(nativeBuffer.get()->device(),nativeDevice.get());
    EXPECT_EQ(nativeVbo.get()->device(),nativeDevice.get());
    EXPECT_EQ(nativeIbo.get()->device(),nativeDevice.get());
    EXPECT_EQ(nativeTexture.get()->device(),nativeDevice.get());

    EXPECT_EQ(MetalApi::borrowDevice(device).get(),nativeDevice.get());
    EXPECT_EQ(MetalApi::borrowBuffer(device,buffer).get(),nativeBuffer.get());
    EXPECT_EQ(MetalApi::borrowBuffer(device,ibo).get(),nativeIbo.get());
    EXPECT_EQ(MetalApi::borrowTexture(device,texture).get(),nativeTexture.get());

    const StorageBuffer emptyBuffer;
    const Texture2d     emptyTexture;
    EXPECT_FALSE(MetalApi::borrowBuffer(device,emptyBuffer));
    EXPECT_FALSE(MetalApi::borrowTexture(device,emptyTexture));

    EXPECT_FALSE(MetalApi::borrowBuffer(foreignDevice,buffer));
    EXPECT_FALSE(MetalApi::borrowTexture(foreignDevice,texture));
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping Metal borrowed native handle testcase: ",e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,RuntimeCompilationCounters) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device device(api);

    const auto initial = MetalApi::runtimeCompilationSnapshot(device);
    ASSERT_TRUE(initial.available);

    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto comp = device.shader("shader/ssbo_read.comp.sprv");
    const auto afterShaders = MetalApi::runtimeCompilationSnapshot(device);
    EXPECT_EQ(afterShaders.sourceLibraryRequests,
              initial.sourceLibraryRequests+3);
    EXPECT_EQ(afterShaders.computePsoRequests,initial.computePsoRequests);
    EXPECT_EQ(afterShaders.renderPsoRequests,initial.renderPsoRequests);

    auto compute = device.pipeline(comp);
    (void)compute;
    const auto afterCompute = MetalApi::runtimeCompilationSnapshot(device);
    EXPECT_EQ(afterCompute.sourceLibraryRequests,
              afterShaders.sourceLibraryRequests);
    EXPECT_EQ(afterCompute.computePsoRequests,
              afterShaders.computePsoRequests+1);
    EXPECT_EQ(afterCompute.renderPsoRequests,afterShaders.renderPsoRequests);

    auto render = device.pipeline(
        Topology::Triangles,RenderState(),vert,frag);
    const auto afterRenderWrapper =
        MetalApi::runtimeCompilationSnapshot(device);
    EXPECT_EQ(afterRenderWrapper.sourceLibraryRequests,
              afterCompute.sourceLibraryRequests);
    EXPECT_EQ(afterRenderWrapper.computePsoRequests,
              afterCompute.computePsoRequests);
    EXPECT_EQ(afterRenderWrapper.renderPsoRequests,
              afterCompute.renderPsoRequests);

    auto target  = device.attachment(TextureFormat::RGBA8,4,4);
    auto command = device.commandBuffer();
    {
      auto encoder = command.startEncoding(device);
      encoder.setFramebuffer(
          {{target,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}});
      encoder.setPipeline(render);
      const auto afterFirstUse =
          MetalApi::runtimeCompilationSnapshot(device);
      EXPECT_EQ(afterFirstUse.renderPsoRequests,
                afterRenderWrapper.renderPsoRequests+1);

      encoder.setPipeline(render);
      const auto afterCachedUse =
          MetalApi::runtimeCompilationSnapshot(device);
      EXPECT_EQ(afterCachedUse.renderPsoRequests,
                afterFirstUse.renderPsoRequests);
      }

    auto sync = device.submit(command);
    sync.wait();
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping Metal runtime compilation counters testcase: ",e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,BuiltinRuntimeCounters) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device device(api);

    const auto initial = MetalApi::builtinRuntimeSnapshot(device);
    ASSERT_TRUE(initial.available);
    for(const uint64_t count:initial.sourceLibraryRequests)
      EXPECT_EQ(count,1);
    for(const uint64_t count:initial.renderPsoRequests)
      EXPECT_EQ(count,0);

    const auto& color   = device.builtin().empty();
    const auto& texture = device.builtin().texture2d();
    const std::array<const RenderPipeline*,12> pipelines = {{
        &color.pen,&color.brush,
        &color.penB,&color.brushB,
        &color.penA,&color.brushA,
        &texture.pen,&texture.brush,
        &texture.penB,&texture.brushB,
        &texture.penA,&texture.brushA,
        }};

    auto target  = device.attachment(TextureFormat::RGBA8,4,4);
    auto command = device.commandBuffer();
    {
      auto encoder = command.startEncoding(device);
      encoder.setFramebuffer(
          {{target,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}});
      for(const RenderPipeline* pipeline:pipelines)
        encoder.setPipeline(*pipeline);
      }

    const auto afterFirstInstances =
        MetalApi::builtinRuntimeSnapshot(device);
    for(const uint64_t count:afterFirstInstances.sourceLibraryRequests)
      EXPECT_EQ(count,1);
    for(const uint64_t count:afterFirstInstances.renderPsoRequests)
      EXPECT_EQ(count,1);

    auto secondTarget  = device.attachment(TextureFormat::RGBA16,4,4);
    auto secondCommand = device.commandBuffer();
    {
      auto encoder = secondCommand.startEncoding(device);
      encoder.setFramebuffer(
          {{secondTarget,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}});
      encoder.setPipeline(color.pen);
      }

    const auto afterSecondInstance =
        MetalApi::builtinRuntimeSnapshot(device);
    EXPECT_EQ(afterSecondInstance.renderPsoRequests[
                  metalBuiltinRenderRoleIndex(
                      MetalBuiltinRenderRole::ColorLinesOpaque)],
              2);
    for(size_t i=1; i<afterSecondInstance.renderPsoRequests.size(); ++i)
      EXPECT_EQ(afterSecondInstance.renderPsoRequests[i],1);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping Metal builtin runtime counters testcase: ",e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,OfflineBuiltinMetallib) {
#if defined(__OSX__)
  try {
    const auto manifest = testOfflineBuiltinManifest();
    MetalApi api{ApiFlags::Validation,manifest};
    Device device(api);

    const auto initialRuntime =
        MetalApi::runtimeCompilationSnapshot(device);
    ASSERT_TRUE(initialRuntime.available);
    EXPECT_EQ(initialRuntime.sourceLibraryRequests,0);

    const auto initialBuiltin =
        MetalApi::builtinRuntimeSnapshot(device);
    ASSERT_TRUE(initialBuiltin.available);
    for(const uint64_t count:initialBuiltin.sourceLibraryRequests)
      EXPECT_EQ(count,0);
    for(const uint64_t count:initialBuiltin.renderPsoRequests)
      EXPECT_EQ(count,0);

    const auto& color   = device.builtin().empty();
    const auto& texture = device.builtin().texture2d();
    const std::array<const RenderPipeline*,12> pipelines = {{
        &color.pen,&color.brush,
        &color.penB,&color.brushB,
        &color.penA,&color.brushA,
        &texture.pen,&texture.brush,
        &texture.penB,&texture.brushB,
        &texture.penA,&texture.brushA,
        }};

    auto target  = device.attachment(TextureFormat::RGBA8,4,4);
    auto command = device.commandBuffer();
    {
      auto encoder = command.startEncoding(device);
      encoder.setFramebuffer(
          {{target,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}});
      for(const RenderPipeline* pipeline:pipelines)
        encoder.setPipeline(*pipeline);
      }

    const auto afterRuntime =
        MetalApi::runtimeCompilationSnapshot(device);
    EXPECT_EQ(afterRuntime.sourceLibraryRequests,0);
    EXPECT_EQ(afterRuntime.computePsoRequests,0);
    EXPECT_EQ(afterRuntime.renderPsoRequests,12);

    const auto afterBuiltin =
        MetalApi::builtinRuntimeSnapshot(device);
    for(const uint64_t count:afterBuiltin.sourceLibraryRequests)
      EXPECT_EQ(count,0);
    for(const uint64_t count:afterBuiltin.renderPsoRequests)
      EXPECT_EQ(count,1);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping offline Metal Builtin testcase: ",e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,OfflineBuiltinPipelineArchiveColdWarmAndRecovery) {
#if defined(__OSX__)
  try {
    const std::string archivePath =
        pipelineArchiveTestPath(".bin");
    const std::string partialArchivePath =
        pipelineArchiveTestPath("-partial.bin");
    const std::string badDirectory =
        pipelineArchiveTestPath("-missing");
    std::error_code cleanupError;
    std::filesystem::remove(archivePath,cleanupError);
    std::filesystem::remove(partialArchivePath,cleanupError);
    std::filesystem::remove_all(badDirectory,cleanupError);

    const auto manifest = testOfflineBuiltinManifest();
    MetalPipelineArchiveConfig archiveConfig;
    archiveConfig.archivePath = archivePath.c_str();

    {
      MetalApi api{
          ApiFlags::Validation,manifest,archiveConfig};
      Device device(api);
      const auto initial =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_NE(initial.flags&
                    MetalPipelineArchiveSnapshot::Configured,0u);
      EXPECT_NE(initial.flags&
                    MetalPipelineArchiveSnapshot::Available,0u);
      EXPECT_NE(initial.flags&
                    MetalPipelineArchiveSnapshot::CreatedEmpty,0u);
      EXPECT_EQ(initial.flags&
                    MetalPipelineArchiveSnapshot::LoadedFromDisk,0u);

      instantiateUnarchivedBuiltinPipeline(device);
      const auto afterUnarchived =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(afterUnarchived.renderHits,0u);
      EXPECT_EQ(afterUnarchived.renderMisses,0u);
      EXPECT_EQ(afterUnarchived.renderAdds,0u);
      EXPECT_EQ(afterUnarchived.renderFallbacks,0u);
      EXPECT_EQ(afterUnarchived.flags&
                    MetalPipelineArchiveSnapshot::Dirty,0u);

      instantiateArchivedBuiltinPipelines(device);
      const auto runtime =
          MetalApi::runtimeCompilationSnapshot(device);
      EXPECT_EQ(runtime.sourceLibraryRequests,0u);
      EXPECT_EQ(runtime.computePsoRequests,0u);
      EXPECT_EQ(runtime.renderPsoRequests,4u);

      const auto cold =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(cold.renderHits,0u);
      EXPECT_EQ(cold.renderMisses,3u);
      EXPECT_EQ(cold.renderAdds,3u);
      EXPECT_EQ(cold.renderFallbacks,0u);
      EXPECT_NE(cold.flags&
                    MetalPipelineArchiveSnapshot::Dirty,0u);
      EXPECT_TRUE(MetalApi::flushPipelineArchive(device));

      const auto flushed =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(flushed.flags&
                    MetalPipelineArchiveSnapshot::Dirty,0u);
      EXPECT_EQ(flushed.flushAttempts,1u);
      EXPECT_EQ(flushed.flushSuccesses,1u);
      EXPECT_EQ(flushed.flushFailures,0u);
      EXPECT_TRUE(std::filesystem::is_regular_file(archivePath));
      EXPECT_GT(std::filesystem::file_size(archivePath),0u);
      }

    {
      MetalApi api{
          ApiFlags::Validation,manifest,archiveConfig};
      Device device(api);
      const auto initial =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_NE(initial.flags&
                    MetalPipelineArchiveSnapshot::LoadedFromDisk,0u);
      EXPECT_EQ(initial.flags&
                    MetalPipelineArchiveSnapshot::CreatedEmpty,0u);

      instantiateArchivedBuiltinPipelines(device);
      const auto runtime =
          MetalApi::runtimeCompilationSnapshot(device);
      EXPECT_EQ(runtime.renderPsoRequests,3u);

      const auto warm =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(warm.renderHits,3u);
      EXPECT_EQ(warm.renderMisses,0u);
      EXPECT_EQ(warm.renderAdds,0u);
      EXPECT_EQ(warm.renderFallbacks,0u);
      EXPECT_TRUE(MetalApi::flushPipelineArchive(device));
      }

    {
      std::ofstream corrupt(
          archivePath,std::ios::binary|std::ios::trunc);
      corrupt << "not a Metal binary archive";
      corrupt.close();

      MetalApi api{
          ApiFlags::Validation,manifest,archiveConfig};
      Device device(api);
      const auto recovered =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(recovered.loadFailures,1u);
      EXPECT_EQ(recovered.rebuilds,1u);
      EXPECT_NE(recovered.flags&
                    MetalPipelineArchiveSnapshot::CreatedEmpty,0u);
      EXPECT_EQ(recovered.flags&
                    MetalPipelineArchiveSnapshot::DisabledAfterError,0u);

      instantiateArchivedBuiltinPipelines(device);
      const auto afterPipelines =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(afterPipelines.renderMisses,3u);
      EXPECT_EQ(afterPipelines.renderAdds,3u);
      EXPECT_TRUE(MetalApi::flushPipelineArchive(device));
      }

    {
      MetalPipelineArchiveConfig partialConfig;
      partialConfig.archivePath = partialArchivePath.c_str();
      MetalApi api{
          ApiFlags::Validation,manifest,partialConfig};
      Device device(api);
      instantiateArchivedBuiltinPipelines(device,true,false,false);
      const auto coldPartial =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(coldPartial.renderHits,0u);
      EXPECT_EQ(coldPartial.renderMisses,1u);
      EXPECT_EQ(coldPartial.renderAdds,1u);
      EXPECT_EQ(
          MetalApi::runtimeCompilationSnapshot(device).
              renderPsoRequests,
          1u);
      EXPECT_TRUE(MetalApi::flushPipelineArchive(device));
      }

    {
      MetalPipelineArchiveConfig partialConfig;
      partialConfig.archivePath = partialArchivePath.c_str();
      MetalApi api{
          ApiFlags::Validation,manifest,partialConfig};
      Device device(api);
      instantiateArchivedBuiltinPipelines(device);
      const auto partial =
          MetalApi::pipelineArchiveSnapshot(device);
      // MTLBinaryArchive stores pipeline functions rather than the whole
      // blend descriptor. Both selected texture roles share the exact same
      // vertex/fragment function pair, so an archive populated through only
      // the opaque role is already a strict hit for the alpha role. The color
      // alpha role uses a distinct function pair and remains a strict miss.
      EXPECT_EQ(partial.renderHits,2u);
      EXPECT_EQ(partial.renderMisses,1u);
      EXPECT_EQ(partial.renderAdds,1u);
      EXPECT_EQ(partial.renderFallbacks,0u);
      EXPECT_EQ(
          MetalApi::runtimeCompilationSnapshot(device).
              renderPsoRequests,
          4u);
      EXPECT_TRUE(MetalApi::flushPipelineArchive(device));
      }

    {
      const std::string unwritablePath =
          badDirectory+"/archive.bin";
      MetalPipelineArchiveConfig unwritableConfig;
      unwritableConfig.archivePath = unwritablePath.c_str();
      MetalApi api{
          ApiFlags::Validation,manifest,unwritableConfig};
      Device device(api);
      instantiateArchivedBuiltinPipelines(device);
      EXPECT_FALSE(MetalApi::flushPipelineArchive(device));

      const auto failed =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(failed.flushAttempts,1u);
      EXPECT_EQ(failed.flushSuccesses,0u);
      EXPECT_EQ(failed.flushFailures,1u);
      EXPECT_NE(failed.flags&
                    MetalPipelineArchiveSnapshot::DisabledAfterError,0u);
      }

    std::filesystem::remove(archivePath,cleanupError);
    std::filesystem::remove(partialArchivePath,cleanupError);
    std::filesystem::remove_all(badDirectory,cleanupError);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping Metal binary archive testcase: ",e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,OfflineInventoryPipelineArchiveColdWarm) {
#if defined(__OSX__)
  try {
    const std::string archivePath =
        pipelineArchiveTestPath("-inventory.bin");
    std::error_code cleanupError;
    std::filesystem::remove(archivePath,cleanupError);

    const OfflineInventorySources sources;
    const auto manifest = testOfflineInventoryManifest(sources);
    MetalPipelineArchiveConfig archiveConfig;
    archiveConfig.archivePath = archivePath.c_str();

    {
      MetalApi api{ApiFlags::Validation,manifest,archiveConfig};
      Device device(api);
      instantiateArchivedInventoryPipeline(device,sources);

      const auto runtime =
          MetalApi::runtimeCompilationSnapshot(device);
      EXPECT_EQ(runtime.sourceLibraryRequests,0u);
      EXPECT_EQ(runtime.computePsoRequests,0u);
      EXPECT_EQ(runtime.renderPsoRequests,1u);

      const auto cold =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(cold.renderHits,0u);
      EXPECT_EQ(cold.renderMisses,1u);
      EXPECT_EQ(cold.renderAdds,1u);
      EXPECT_EQ(cold.renderFallbacks,0u);
      EXPECT_NE(cold.flags&
                    MetalPipelineArchiveSnapshot::Dirty,0u);
      EXPECT_TRUE(MetalApi::flushPipelineArchive(device));
      EXPECT_TRUE(std::filesystem::is_regular_file(archivePath));
      EXPECT_GT(std::filesystem::file_size(archivePath),0u);
    }

    {
      MetalApi api{ApiFlags::Validation,manifest,archiveConfig};
      Device device(api);
      const auto initial =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_NE(initial.flags&
                    MetalPipelineArchiveSnapshot::LoadedFromDisk,0u);

      instantiateArchivedInventoryPipeline(device,sources);
      const auto runtime =
          MetalApi::runtimeCompilationSnapshot(device);
      EXPECT_EQ(runtime.sourceLibraryRequests,0u);
      EXPECT_EQ(runtime.computePsoRequests,0u);
      EXPECT_EQ(runtime.renderPsoRequests,1u);

      const auto warm =
          MetalApi::pipelineArchiveSnapshot(device);
      EXPECT_EQ(warm.renderHits,1u);
      EXPECT_EQ(warm.renderMisses,0u);
      EXPECT_EQ(warm.renderAdds,0u);
      EXPECT_EQ(warm.renderFallbacks,0u);
      EXPECT_EQ(warm.flags&
                    MetalPipelineArchiveSnapshot::Dirty,0u);
      EXPECT_TRUE(MetalApi::flushPipelineArchive(device));
    }

    std::filesystem::remove(archivePath,cleanupError);
  }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping Metal inventory binary archive testcase: ",e.what());
    else
      throw;
  }
#endif
  }

TEST(MetalApi,OfflineBuiltinManifestFailsClosed) {
#if defined(__OSX__)
  try {
    MetalApi availableApi;
    Device availableDevice(availableApi);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice) {
      Log::d("Skipping offline Metal Builtin fail-closed testcase: ",
             e.what());
      return;
      }
    throw;
    }

  auto missingLibrary = testOfflineBuiltinManifest();
  missingLibrary.metallibPath =
      "/tmp/tempest-does-not-exist/metal_builtin_offline.metallib";
  MetalApi missingLibraryApi{ApiFlags::NoFlags,missingLibrary};
  try {
    Device device(missingLibraryApi);
    FAIL() << "missing offline metallib unexpectedly loaded";
    }
  catch(const std::system_error& e) {
    EXPECT_EQ(e.code(),Tempest::GraphicsErrc::InvalidShaderModule);
    }

  auto missingFunction = testOfflineBuiltinManifest();
  missingFunction.textureFragmentFunction =
      "tempestOfflineMissingTextureFragment";
  MetalApi missingFunctionApi{ApiFlags::NoFlags,missingFunction};
  try {
    Device device(missingFunctionApi);
    FAIL() << "missing offline Builtin function unexpectedly fell back";
    }
  catch(const std::system_error& e) {
    EXPECT_EQ(e.code(),Tempest::GraphicsErrc::InvalidShaderModule);
    }

  auto wrongStage = testOfflineBuiltinManifest();
  wrongStage.textureFragmentFunction =
      "tempestOfflineWrongFragmentStage";
  MetalApi wrongStageApi{ApiFlags::NoFlags,wrongStage};
  try {
    Device device(wrongStageApi);
    FAIL() << "wrong-stage offline Builtin function was accepted";
    }
  catch(const std::system_error& e) {
    EXPECT_EQ(e.code(),Tempest::GraphicsErrc::InvalidShaderModule);
    }
#endif
  }

TEST(MetalApi,OfflineInventoryMetallib) {
#if defined(__OSX__)
  try {
    const OfflineInventorySources sources;
    const auto manifest = testOfflineInventoryManifest(sources);
    MetalApi api{ApiFlags::Validation,manifest};
    Device device(api);

    const auto initial =
        MetalApi::runtimeCompilationSnapshot(device);
    ASSERT_TRUE(initial.available);
    EXPECT_EQ(initial.sourceLibraryRequests,0);
    EXPECT_EQ(initial.computePsoRequests,0);
    EXPECT_EQ(initial.renderPsoRequests,0);

    auto vert = device.shader(
        sources.vertex.data(),sources.vertex.size()*sizeof(uint32_t));
    auto frag = device.shader(
        sources.fragment.data(),sources.fragment.size()*sizeof(uint32_t));
    const auto afterShaders =
        MetalApi::runtimeCompilationSnapshot(device);
    EXPECT_EQ(afterShaders.sourceLibraryRequests,0);
    EXPECT_EQ(afterShaders.computePsoRequests,0);
    EXPECT_EQ(afterShaders.renderPsoRequests,0);

    RenderState state;
    state.setCullFaceMode(RenderState::CullMode::NoCull);
    auto pipeline =
        device.pipeline(Topology::Triangles,state,vert,frag);
    const auto afterWrapper =
        MetalApi::runtimeCompilationSnapshot(device);
    EXPECT_EQ(afterWrapper.sourceLibraryRequests,0);
    EXPECT_EQ(afterWrapper.computePsoRequests,0);
    EXPECT_EQ(afterWrapper.renderPsoRequests,0);

    auto target  = device.attachment(TextureFormat::RGBA8,4,4);
    auto command = device.commandBuffer();
    {
      auto encoder = command.startEncoding(device);
      encoder.setFramebuffer(
          {{target,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}});
      encoder.setPipeline(pipeline);
    }

    const auto afterFirstUse =
        MetalApi::runtimeCompilationSnapshot(device);
    EXPECT_EQ(afterFirstUse.sourceLibraryRequests,0);
    EXPECT_EQ(afterFirstUse.computePsoRequests,0);
    EXPECT_EQ(afterFirstUse.renderPsoRequests,1);
  }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping offline Metal inventory testcase: ",e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,OfflineInventoryManifestFailsClosed) {
#if defined(__OSX__)
  try {
    MetalApi availableApi;
    Device availableDevice(availableApi);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice) {
      Log::d("Skipping offline Metal inventory fail-closed testcase: ",
             e.what());
      return;
      }
    throw;
    }

  const OfflineInventorySources sources;

  auto missingFunction = testOfflineInventoryManifest(sources);
  missingFunction.inventoryFragmentFunction =
      "tempestOfflineMissingInventoryFragment";
  MetalApi missingFunctionApi{ApiFlags::NoFlags,missingFunction};
  Device missingFunctionDevice(missingFunctionApi);
  try {
    (void)missingFunctionDevice.shader(
        sources.fragment.data(),
        sources.fragment.size()*sizeof(uint32_t));
    FAIL() << "missing offline inventory function unexpectedly fell back";
    }
  catch(const std::system_error& e) {
    EXPECT_EQ(e.code(),Tempest::GraphicsErrc::InvalidShaderModule);
    }
  const auto afterMissingFunction =
      MetalApi::runtimeCompilationSnapshot(missingFunctionDevice);
  EXPECT_EQ(afterMissingFunction.sourceLibraryRequests,0);

  auto wrongStage = testOfflineInventoryManifest(sources);
  wrongStage.inventoryFragmentFunction =
      "tempestOfflineWrongInventoryFragmentStage";
  MetalApi wrongStageApi{ApiFlags::NoFlags,wrongStage};
  Device wrongStageDevice(wrongStageApi);
  try {
    (void)wrongStageDevice.shader(
        sources.fragment.data(),
        sources.fragment.size()*sizeof(uint32_t));
    FAIL() << "wrong-stage offline inventory function was accepted";
    }
  catch(const std::system_error& e) {
    EXPECT_EQ(e.code(),Tempest::GraphicsErrc::InvalidShaderModule);
    }
  const auto afterWrongStage =
      MetalApi::runtimeCompilationSnapshot(wrongStageDevice);
  EXPECT_EQ(afterWrongStage.sourceLibraryRequests,0);
#endif
  }

TEST(MetalApi,ActiveRenderEncoderScope) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device device(api);
    Device foreignDevice(api);

    auto vbo  = device.vbo(GapiTestCommon::vboData,3);
    auto ibo  = device.ibo(GapiTestCommon::iboData,3);
    auto vert = device.shader("shader/simple_test.vert.sprv");
    auto frag = device.shader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline(
        Topology::Triangles,RenderState(),vert,frag);
    auto target  = device.attachment(TextureFormat::RGBA8,4,4);
    auto command = device.commandBuffer();
    {
      auto encoder = command.startEncoding(device);
      bool called = false;
      EXPECT_FALSE(MetalApi::withActiveRenderEncoder(
          device,encoder,&called,observeActiveRenderEncoder));
      EXPECT_FALSE(called);

      encoder.setFramebuffer(
          {{target,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}});
      encoder.setPipeline(pso);
      encoder.draw(vbo,ibo);

      EXPECT_FALSE(MetalApi::withActiveRenderEncoder(
          device,encoder,nullptr,nullptr));
      EXPECT_FALSE(MetalApi::withActiveRenderEncoder(
          foreignDevice,encoder,&called,observeActiveRenderEncoder));
      EXPECT_FALSE(called);

      EXPECT_TRUE(MetalApi::withActiveRenderEncoder(
          device,encoder,&called,observeActiveRenderEncoder));
      EXPECT_TRUE(called);
      encoder.setPipeline(pso);
      encoder.draw(vbo,ibo);

      EXPECT_THROW(
          (void)MetalApi::withActiveRenderEncoder(
              device,encoder,nullptr,throwFromActiveRenderEncoder),
          std::runtime_error);
      encoder.setPipeline(pso);
      encoder.draw(vbo,ibo);

      called = false;
      EXPECT_TRUE(MetalApi::withActiveRenderEncoder(
          device,encoder,&called,observeActiveRenderEncoder));
      EXPECT_TRUE(called);
      encoder.setPipeline(pso);
      encoder.draw(vbo,ibo);
      }

    auto sync = device.submit(command);
    sync.wait();
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping Metal active render encoder testcase: ",e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,ActiveCommandBufferScope) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device device(api);
    Device foreignDevice(api);

    auto target = device.attachment(TextureFormat::RGBA8,4,4);
    const auto nativeTarget = MetalApi::borrowTexture(
        device,textureCast<const Texture2d&>(target));
    ASSERT_TRUE(nativeTarget);

    auto command = device.commandBuffer();
    NativeClearContext clear{nativeTarget.get()};
    {
      auto encoder = command.startEncoding(device);
      bool called = false;
      EXPECT_FALSE(MetalApi::withActiveCommandBuffer(
          device,encoder,&called,nullptr));
      nullContextCommandBufferCallbackCalled = false;
      EXPECT_FALSE(MetalApi::withActiveCommandBuffer(
          device,encoder,nullptr,observeNullContextCommandBuffer));
      EXPECT_FALSE(nullContextCommandBufferCallbackCalled);
      EXPECT_FALSE(MetalApi::withActiveCommandBuffer(
          foreignDevice,encoder,&called,observeActiveCommandBuffer));
      EXPECT_FALSE(called);

      EXPECT_TRUE(MetalApi::withActiveCommandBuffer(
          device,encoder,&clear,encodeNativeClearPasses));
      EXPECT_EQ(clear.encodedPasses,2u);

      bool secondCallInvoked = false;
      EXPECT_FALSE(MetalApi::withActiveCommandBuffer(
          device,encoder,&secondCallInvoked,observeActiveCommandBuffer));
      EXPECT_FALSE(secondCallInvoked);
    }

    auto sync = device.submit(command);
    EXPECT_TRUE(sync.wait(5000));

    auto activeCommand = device.commandBuffer();
    {
      auto encoder = activeCommand.startEncoding(device);
      encoder.setFramebuffer(
          {{target,Vec4(0.f,0.f,0.f,1.f),Tempest::Preserve}});
      bool called = false;
      EXPECT_FALSE(MetalApi::withActiveCommandBuffer(
          device,encoder,&called,observeActiveCommandBuffer));
      EXPECT_FALSE(called);
    }

    auto activeSync = device.submit(activeCommand);
    EXPECT_TRUE(activeSync.wait(5000));
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping Metal active command buffer testcase: ",e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,SsboCopy) {
#if defined(__OSX__)
  GapiTestCommon::SsboCopy<MetalApi,TextureFormat::RGBA8,uint8_t>();
#endif
  }

TEST(MetalApi,SsboEmpty) {
#if defined(__OSX__)
  GapiTestCommon::SsboEmpty<MetalApi>();
#endif
  }

TEST(MetalApi,ArrayLength) {
#if defined(__OSX__)
  GapiTestCommon::ArrayLength<MetalApi>();
#endif
  }

TEST(MetalApi,NonSampledTexture) {
#if defined(__OSX__)
  GapiTestCommon::NonSampledTexture<MetalApi>("MetalApi_NonSampledTexture.png");
#endif
  }

TEST(MetalApi,Shader) {
#if defined(__OSX__)
  GapiTestCommon::Shader<MetalApi>();
#endif
  }

TEST(MetalApi,Pso) {
#if defined(__OSX__)
  GapiTestCommon::Pso<MetalApi>();
#endif
  }

TEST(MetalApi,PsoInconsistentVaryings) {
#if defined(__OSX__)
  GapiTestCommon::PsoInconsistentVaryings<MetalApi>();
#endif
  }

TEST(MetalApi,Fbo) {
#if defined(__OSX__)
  GapiTestCommon::Fbo<MetalApi>("MetalApi_Fbo.png");
#endif
  }

TEST(MetalApi,Draw) {
#if defined(__OSX__)
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGBA8>  ("MetalApi_Draw_RGBA8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGB8>   ("MetalApi_Draw_RGB8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RG8>    ("MetalApi_Draw_RG8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::R8>     ("MetalApi_Draw_R8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGBA16> ("MetalApi_Draw_RGBA16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGB16>  ("MetalApi_Draw_RGB16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RG16>   ("MetalApi_Draw_RG16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::R16>    ("MetalApi_Draw_R16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGBA32F>("MetalApi_Draw_RGBA32F.hdr");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGB32F> ("MetalApi_Draw_RGB32F.hdr");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RG32F>  ("MetalApi_Draw_RG32F.hdr");
  GapiTestCommon::Draw<MetalApi,TextureFormat::R32F>   ("MetalApi_Draw_R32F.hdr");
#endif
  }

TEST(MetalApi,DepthWrite) {
#if defined(__OSX__)
  GapiTestCommon::DepthWrite<MetalApi>("MetalApi_DepthWrite.png");
#endif
  }

TEST(MetalApi,InstanceIndex) {
#if defined(__OSX__)
  GapiTestCommon::InstanceIndex<MetalApi>("MetalApi_InstanceIndex.png");
#endif
  }

TEST(MetalApi,Viewport) {
#if defined(__OSX__)
  GapiTestCommon::Viewport<MetalApi>("MetalApi_Viewport.png");
#endif
  }

TEST(MetalApi,Uniforms) {
#if defined(__OSX__)
  GapiTestCommon::Uniforms<MetalApi>("MetalApi_Uniforms_UBO.png", true);
  GapiTestCommon::Uniforms<MetalApi>("MetalApi_Uniforms_SSBO.png",false);
#endif
  }

TEST(MetalApi,SsboOverlap) {
#if defined(__OSX__)
  GapiTestCommon::SsboOverlap<MetalApi>();
#endif
  }

TEST(MetalApi,Compute) {
#if defined(__OSX__)
  GapiTestCommon::Compute<MetalApi>();
#endif
  }

TEST(MetalApi,ComputeImage) {
#if defined(__OSX__)
  GapiTestCommon::ComputeImage<MetalApi>("MetalApi_ComputeImage.png");
#endif
  }

TEST(MetalApi,AtomicImage) {
#if defined(__OSX__)
  GapiTestCommon::AtomicImage<MetalApi>("MetalApi_AtomicImage.png");
#endif
  }

TEST(MetalApi,AtomicImage3D) {
#if defined(__OSX__)
  GapiTestCommon::AtomicImage3D<MetalApi>("MetalApi_AtomicImage3D.png");
#endif
  }

TEST(MetalApi,MipMaps) {
#if defined(__OSX__)
  GapiTestCommon::MipMaps<MetalApi,TextureFormat::RGBA8>  ("MetalApi_MipMaps_RGBA8.png");
  GapiTestCommon::MipMaps<MetalApi,TextureFormat::RGBA16> ("MetalApi_MipMaps_RGBA16.png");
  GapiTestCommon::MipMaps<MetalApi,TextureFormat::RGBA32F>("MetalApi_MipMaps_RGBA32.png");
#endif
  }

TEST(MetalApi,S3TC) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device       device(api);

    auto tex = device.texture("assets/pixmap_io/dxt5.dds");
    EXPECT_EQ(tex.format(),TextureFormat::DXT5);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,PsoTess) {
#if defined(__OSX__)
  GapiTestCommon::PsoTess<MetalApi>();
#endif
  }

TEST(MetalApi,DISABLED_TesselationBasic) {
#if defined(__OSX__)
  GapiTestCommon::TesselationBasic<MetalApi>("MetalApi_TesselationBasic.png");
#endif
  }

TEST(MetalApi,SsboWrite) {
#if defined(__OSX__)
  GapiTestCommon::SsboWrite<MetalApi>();
#endif
  }

TEST(MetalApi,UnboundSsbo) {
#if defined(__OSX__)
  GapiTestCommon::UnboundSsbo<MetalApi>();
#endif
  }

TEST(MetalApi,ComponentSwizzle) {
#if defined(__OSX__)
  GapiTestCommon::ComponentSwizzle<MetalApi>();
#endif
  }

TEST(MetalApi,PushRemapping) {
#if defined(__OSX__)
  GapiTestCommon::PushRemapping<MetalApi>();
#endif
  }

TEST(MetalApi,Bindless) {
#if defined(__OSX__)
  GapiTestCommon::Bindless<MetalApi>("MetalApi_Bindless.png");
#endif
  }

TEST(MetalApi,Bindless2) {
#if defined(__OSX__)
  GapiTestCommon::Bindless2<MetalApi>("MetalApi_Bindless2.png");
#endif
  }

TEST(MetalApi,UnusedDescriptor) {
#if defined(__OSX__)
  GapiTestCommon::UnusedDescriptor<MetalApi>("MetalApi_UnusedDescriptor.png");
#endif
  }

TEST(MetalApi,Blas) {
#if defined(__OSX__)
  GapiTestCommon::Blas<MetalApi>();
#endif
  }

TEST(MetalApi,RayQuery) {
#if defined(__OSX__)
  GapiTestCommon::RayQuery<MetalApi>("MetalApi_RayQuery.png");
#endif
  }

TEST(MetalApi,DISABLED_RayQueryFace) {
#if defined(__OSX__)
  GapiTestCommon::RayQueryFace<MetalApi>("MetalApi_RayQueryFace.png");
#endif
  }

TEST(MetalApi,DISABLED_MeshShader) {
#if defined(__OSX__)
  GapiTestCommon::MeshShader<MetalApi>("MetalApi_MeshShader.png");
#endif
  }

TEST(MetalApi,DispathIndirect) {
#if defined(__OSX__)
  GapiTestCommon::DispathIndirect<MetalApi>();
#endif
  }
