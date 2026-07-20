#include "../../../Engine/gapi/metal/mtbuiltinruntime.h"
#include "../../../Engine/gapi/metal/mtpipelinearchive.h"

#include "builtin_shader.h"

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using namespace Tempest;

namespace {

using Tempest::Detail::classifyMetalBuiltinRenderRole;
using Tempest::Detail::classifyMetalBuiltinSource;
using Tempest::Detail::classifyMetalInventoryOfflineSource;
using Tempest::Detail::configureMetalBuiltinOfflineReflection;
using Tempest::Detail::configureMetalInventoryOfflineReflection;
using Tempest::Detail::makeMetalBuiltinOfflineConfig;
using Tempest::Detail::makeMetalPipelineArchiveConfig;

template<size_t N>
void expectExactSourceRole(const uint8_t (&source)[N],
                           MetalBuiltinSourceRole expected) {
  EXPECT_EQ(classifyMetalBuiltinSource(source,N),expected);

  std::vector<uint8_t> mutated(source,source+N);
  mutated[N-1] ^= 1;
  EXPECT_EQ(classifyMetalBuiltinSource(mutated.data(),mutated.size()),
            MetalBuiltinSourceRole::None);
  EXPECT_EQ(classifyMetalBuiltinSource(source,N-1),
            MetalBuiltinSourceRole::None);
  }

RenderState alphaBlend() {
  RenderState state;
  state.setBlendSource(RenderState::BlendMode::SrcAlpha);
  state.setBlendDest(RenderState::BlendMode::OneMinusSrcAlpha);
  return state;
  }

RenderState additiveBlend() {
  RenderState state;
  state.setBlendSource(RenderState::BlendMode::One);
  state.setBlendDest(RenderState::BlendMode::One);
  return state;
  }

MetalBuiltinOfflineManifest validOfflineManifest() {
  MetalBuiltinOfflineManifest manifest;
  manifest.metallibPath            = "/tmp/tempest-builtins.metallib";
  manifest.colorVertexFunction     = "colorVertex";
  manifest.colorFragmentFunction   = "colorFragment";
  manifest.textureVertexFunction   = "textureVertex";
  manifest.textureFragmentFunction = "textureFragment";
  return manifest;
  }

constexpr std::array<uint32_t,5> inventoryVertexSource = {
    0x07230203,0x00010000,0,1,1,
    };
constexpr std::array<uint32_t,5> inventoryFragmentSource = {
    0x07230203,0x00010000,0,1,2,
    };

void addInventoryPair(MetalBuiltinOfflineManifest& manifest) {
  manifest.inventoryVertexSpirv = inventoryVertexSource.data();
  manifest.inventoryVertexSpirvSize = sizeof(inventoryVertexSource);
  manifest.inventoryVertexFunction = "inventoryVertex";
  manifest.inventoryFragmentSpirv = inventoryFragmentSource.data();
  manifest.inventoryFragmentSpirvSize = sizeof(inventoryFragmentSource);
  manifest.inventoryFragmentFunction = "inventoryFragment";
  }

std::vector<Decl::ComponentType> builtinVertexDecl() {
  return {
      Decl::ComponentType::float3,
      Decl::ComponentType::float2,
      Decl::ComponentType::float4,
      };
  }

Detail::ShaderReflection::Binding builtinTextureBinding() {
  Detail::ShaderReflection::Binding binding;
  binding.layout    = 0;
  binding.cls       = Detail::ShaderReflection::Class::Texture;
  binding.stage     = Detail::ShaderReflection::Stage::Fragment;
  binding.arraySize = 1;
  return binding;
  }

}

TEST(MetalBuiltinRuntime,OwnsAndMapsValidOfflineManifest) {
  std::string path = "/tmp/tempest-builtins.metallib";
  std::array<std::string,4> names = {{
      "colorVertex",
      "colorFragment",
      "textureVertex",
      "textureFragment",
      }};
  MetalBuiltinOfflineManifest manifest;
  manifest.metallibPath            = path.c_str();
  manifest.colorVertexFunction     = names[0].c_str();
  manifest.colorFragmentFunction   = names[1].c_str();
  manifest.textureVertexFunction   = names[2].c_str();
  manifest.textureFragmentFunction = names[3].c_str();
  std::array<uint32_t,5> inventoryVertex = inventoryVertexSource;
  std::array<uint32_t,5> inventoryFragment = inventoryFragmentSource;
  std::array<std::string,2> inventoryNames = {{
      "inventoryVertex",
      "inventoryFragment",
      }};
  manifest.inventoryVertexSpirv = inventoryVertex.data();
  manifest.inventoryVertexSpirvSize = sizeof(inventoryVertex);
  manifest.inventoryVertexFunction = inventoryNames[0].c_str();
  manifest.inventoryFragmentSpirv = inventoryFragment.data();
  manifest.inventoryFragmentSpirvSize = sizeof(inventoryFragment);
  manifest.inventoryFragmentFunction = inventoryNames[1].c_str();

  const auto config = makeMetalBuiltinOfflineConfig(manifest);
  path[1] = 'X';
  for(auto& name:names)
    name[0] = 'X';
  inventoryVertex[4] = 0;
  inventoryFragment[4] = 0;
  for(auto& name:inventoryNames)
    name[0] = 'X';

  EXPECT_EQ(config->metallibPath,"/tmp/tempest-builtins.metallib");
  EXPECT_EQ(config->functionNames[metalBuiltinSourceRoleIndex(
                MetalBuiltinSourceRole::ColorVertex)],"colorVertex");
  EXPECT_EQ(config->functionNames[metalBuiltinSourceRoleIndex(
                MetalBuiltinSourceRole::ColorFragment)],"colorFragment");
  EXPECT_EQ(config->functionNames[metalBuiltinSourceRoleIndex(
                MetalBuiltinSourceRole::TextureVertex)],"textureVertex");
  EXPECT_EQ(config->functionNames[metalBuiltinSourceRoleIndex(
                MetalBuiltinSourceRole::TextureFragment)],"textureFragment");
  ASSERT_EQ(config->inventorySources[0].size(),
            sizeof(inventoryVertexSource));
  ASSERT_EQ(config->inventorySources[1].size(),
            sizeof(inventoryFragmentSource));
  EXPECT_EQ(std::memcmp(config->inventorySources[0].data(),
                        inventoryVertexSource.data(),
                        sizeof(inventoryVertexSource)),0);
  EXPECT_EQ(std::memcmp(config->inventorySources[1].data(),
                        inventoryFragmentSource.data(),
                        sizeof(inventoryFragmentSource)),0);
  EXPECT_EQ(config->inventoryFunctionNames[0],"inventoryVertex");
  EXPECT_EQ(config->inventoryFunctionNames[1],"inventoryFragment");
  }

TEST(MetalPipelineArchive,PublicAbiAndOwnership) {
  static_assert(MetalPipelineArchiveConfig::AbiVersion==1);
  static_assert(MetalPipelineArchiveSnapshot::AbiVersion==1);
  static_assert(std::is_standard_layout_v<MetalPipelineArchiveConfig>);
  static_assert(std::is_trivially_copyable_v<MetalPipelineArchiveConfig>);
  static_assert(std::is_standard_layout_v<MetalPipelineArchiveSnapshot>);
  static_assert(std::is_trivially_copyable_v<MetalPipelineArchiveSnapshot>);

  std::string path = "/tmp/tempest-pipeline-archive.bin";
  MetalPipelineArchiveConfig config;
  config.archivePath = path.c_str();
  const auto owned = makeMetalPipelineArchiveConfig(config);
  path[1] = 'X';

  EXPECT_EQ(owned->archivePath,
            "/tmp/tempest-pipeline-archive.bin");
  MetalPipelineArchiveSnapshot snapshot;
  EXPECT_EQ(snapshot.abiVersion,
            MetalPipelineArchiveSnapshot::AbiVersion);
  EXPECT_EQ(snapshot.structSize,
            MetalPipelineArchiveSnapshot::StructSize);
  EXPECT_EQ(snapshot.flags,0u);
  EXPECT_EQ(snapshot.renderHits,0u);
  EXPECT_EQ(snapshot.computeFallbacks,0u);
  EXPECT_EQ(snapshot.flushFailures,0u);
  }

TEST(MetalPipelineArchive,RejectsInvalidConfig) {
  MetalPipelineArchiveConfig config;
  config.archivePath = "/tmp/tempest-pipeline-archive.bin";

  config.abiVersion++;
  EXPECT_THROW((void)makeMetalPipelineArchiveConfig(config),
               std::invalid_argument);
  config.abiVersion = MetalPipelineArchiveConfig::AbiVersion;

  config.structSize--;
  EXPECT_THROW((void)makeMetalPipelineArchiveConfig(config),
               std::invalid_argument);
  config.structSize = MetalPipelineArchiveConfig::StructSize;

  config.archivePath = nullptr;
  EXPECT_THROW((void)makeMetalPipelineArchiveConfig(config),
               std::invalid_argument);
  config.archivePath = "";
  EXPECT_THROW((void)makeMetalPipelineArchiveConfig(config),
               std::invalid_argument);
  config.archivePath = "relative.bin";
  EXPECT_THROW((void)makeMetalPipelineArchiveConfig(config),
               std::invalid_argument);

  const char invalidUtf8[] = {'/',char(0xc0),char(0x80),0};
  config.archivePath = invalidUtf8;
  EXPECT_THROW((void)makeMetalPipelineArchiveConfig(config),
               std::invalid_argument);
  }

TEST(MetalPipelineArchive,HardCodesOnlySelectedRenderRoles) {
  for(size_t i=0;
      i<metalBuiltinRenderRoleIndex(MetalBuiltinRenderRole::Count);
      ++i) {
    const auto role = static_cast<MetalBuiltinRenderRole>(i);
    const bool expected =
        role==MetalBuiltinRenderRole::ColorTrianglesAlpha ||
        role==MetalBuiltinRenderRole::TextureTrianglesOpaque ||
        role==MetalBuiltinRenderRole::TextureTrianglesAlpha;
    EXPECT_EQ(Tempest::Detail::isMetalPipelineArchiveRenderRole(role),
              expected);
    }
  EXPECT_FALSE(Tempest::Detail::isMetalPipelineArchiveRenderRole(
      MetalBuiltinRenderRole::None));
  }

TEST(MetalBuiltinRuntime,RejectsInvalidOfflineManifest) {
  auto manifest = validOfflineManifest();
  manifest.abiVersion++;
  EXPECT_THROW((void)makeMetalBuiltinOfflineConfig(manifest),
               std::invalid_argument);

  manifest = validOfflineManifest();
  manifest.structSize--;
  EXPECT_THROW((void)makeMetalBuiltinOfflineConfig(manifest),
               std::invalid_argument);

  manifest = validOfflineManifest();
  manifest.metallibPath = "relative.metallib";
  EXPECT_THROW((void)makeMetalBuiltinOfflineConfig(manifest),
               std::invalid_argument);

  manifest = validOfflineManifest();
  manifest.textureFragmentFunction = nullptr;
  EXPECT_THROW((void)makeMetalBuiltinOfflineConfig(manifest),
               std::invalid_argument);

  manifest = validOfflineManifest();
  manifest.textureFragmentFunction = manifest.textureVertexFunction;
  EXPECT_THROW((void)makeMetalBuiltinOfflineConfig(manifest),
               std::invalid_argument);

  manifest = validOfflineManifest();
  manifest.inventoryVertexSpirv = inventoryVertexSource.data();
  manifest.inventoryVertexSpirvSize = sizeof(inventoryVertexSource);
  EXPECT_THROW((void)makeMetalBuiltinOfflineConfig(manifest),
               std::invalid_argument);

  manifest = validOfflineManifest();
  addInventoryPair(manifest);
  manifest.inventoryFragmentSpirv = manifest.inventoryVertexSpirv;
  manifest.inventoryFragmentSpirvSize = manifest.inventoryVertexSpirvSize;
  EXPECT_THROW((void)makeMetalBuiltinOfflineConfig(manifest),
               std::invalid_argument);

  manifest = validOfflineManifest();
  addInventoryPair(manifest);
  manifest.inventoryFragmentFunction = manifest.inventoryVertexFunction;
  EXPECT_THROW((void)makeMetalBuiltinOfflineConfig(manifest),
               std::invalid_argument);
  }

TEST(MetalBuiltinRuntime,ClassifiesExactInventoryManifestSources) {
  auto manifest = validOfflineManifest();
  addInventoryPair(manifest);
  const auto config = makeMetalBuiltinOfflineConfig(manifest);

  EXPECT_EQ(classifyMetalInventoryOfflineSource(
                *config,inventoryVertexSource.data(),
                sizeof(inventoryVertexSource)),
            Detail::MetalInventoryOfflineRole::Vertex);
  EXPECT_EQ(classifyMetalInventoryOfflineSource(
                *config,inventoryFragmentSource.data(),
                sizeof(inventoryFragmentSource)),
            Detail::MetalInventoryOfflineRole::Fragment);
  EXPECT_EQ(classifyMetalInventoryOfflineSource(
                *config,inventoryVertexSource.data(),
                sizeof(inventoryVertexSource)-sizeof(uint32_t)),
            Detail::MetalInventoryOfflineRole::None);

  auto mutated = inventoryVertexSource;
  mutated.back() ^= 1u;
  EXPECT_EQ(classifyMetalInventoryOfflineSource(
                *config,mutated.data(),sizeof(mutated)),
            Detail::MetalInventoryOfflineRole::None);
  EXPECT_EQ(classifyMetalInventoryOfflineSource(*config,nullptr,0),
            Detail::MetalInventoryOfflineRole::None);
  }

TEST(MetalBuiltinRuntime,ValidatesAndConfiguresOfflineReflection) {
  const auto vertexDecl = builtinVertexDecl();
  std::vector<Detail::ShaderReflection::Binding> noBindings;
  EXPECT_TRUE(configureMetalBuiltinOfflineReflection(
      MetalBuiltinSourceRole::ColorVertex,
      Detail::ShaderReflection::Stage::Vertex,
      vertexDecl,noBindings));
  EXPECT_TRUE(configureMetalBuiltinOfflineReflection(
      MetalBuiltinSourceRole::TextureVertex,
      Detail::ShaderReflection::Stage::Vertex,
      vertexDecl,noBindings));
  EXPECT_TRUE(configureMetalBuiltinOfflineReflection(
      MetalBuiltinSourceRole::ColorFragment,
      Detail::ShaderReflection::Stage::Fragment,
      {},noBindings));

  std::vector<Detail::ShaderReflection::Binding> textureBindings = {
      builtinTextureBinding(),
      };
  EXPECT_TRUE(configureMetalBuiltinOfflineReflection(
      MetalBuiltinSourceRole::TextureFragment,
      Detail::ShaderReflection::Stage::Fragment,
      {},textureBindings));
  ASSERT_EQ(textureBindings.size(),1);
  EXPECT_EQ(textureBindings[0].mslBinding,0);
  EXPECT_EQ(textureBindings[0].mslBinding2,0);
  EXPECT_EQ(textureBindings[0].mslSize,0);
  }

TEST(MetalBuiltinRuntime,RejectsMismatchedOfflineReflection) {
  auto vertexDecl = builtinVertexDecl();
  std::vector<Detail::ShaderReflection::Binding> noBindings;
  EXPECT_FALSE(configureMetalBuiltinOfflineReflection(
      MetalBuiltinSourceRole::None,
      Detail::ShaderReflection::Stage::Vertex,
      vertexDecl,noBindings));
  EXPECT_FALSE(configureMetalBuiltinOfflineReflection(
      MetalBuiltinSourceRole::ColorVertex,
      Detail::ShaderReflection::Stage::Fragment,
      vertexDecl,noBindings));

  vertexDecl[1] = Decl::ComponentType::float3;
  EXPECT_FALSE(configureMetalBuiltinOfflineReflection(
      MetalBuiltinSourceRole::ColorVertex,
      Detail::ShaderReflection::Stage::Vertex,
      vertexDecl,noBindings));

  auto binding = builtinTextureBinding();
  binding.layout = 1;
  std::vector<Detail::ShaderReflection::Binding> wrongBindings = {binding};
  EXPECT_FALSE(configureMetalBuiltinOfflineReflection(
      MetalBuiltinSourceRole::TextureFragment,
      Detail::ShaderReflection::Stage::Fragment,
      {},wrongBindings));
  EXPECT_FALSE(configureMetalBuiltinOfflineReflection(
      MetalBuiltinSourceRole::ColorFragment,
      Detail::ShaderReflection::Stage::Fragment,
      {},wrongBindings));
  }

TEST(MetalBuiltinRuntime,ValidatesInventoryOfflineReflection) {
  using Binding = Detail::ShaderReflection::Binding;
  using Class = Detail::ShaderReflection::Class;
  using Stage = Detail::ShaderReflection::Stage;
  const std::vector<Decl::ComponentType> vertexDecl = {
      Decl::ComponentType::float3,
      Decl::ComponentType::float3,
      Decl::ComponentType::float2,
      Decl::ComponentType::uint1,
      };
  Binding push;
  push.layout = 0;
  push.cls = Class::Push;
  push.stage = Stage::Vertex;
  push.byteSize = 64;
  std::vector<Binding> vertexLayout = {push};
  EXPECT_TRUE(configureMetalInventoryOfflineReflection(
      Detail::MetalInventoryOfflineRole::Vertex,
      Stage::Vertex,vertexDecl,vertexLayout));
  EXPECT_EQ(vertexLayout[0].mslBinding,0);
  EXPECT_EQ(vertexLayout[0].mslBinding2,uint32_t(-1));
  EXPECT_EQ(vertexLayout[0].mslSize,64);

  Binding texture = builtinTextureBinding();
  std::vector<Binding> fragmentLayout = {texture};
  EXPECT_TRUE(configureMetalInventoryOfflineReflection(
      Detail::MetalInventoryOfflineRole::Fragment,
      Stage::Fragment,{},fragmentLayout));
  EXPECT_EQ(fragmentLayout[0].mslBinding,0);
  EXPECT_EQ(fragmentLayout[0].mslBinding2,0);

  vertexLayout = {push};
  auto wrongDecl = vertexDecl;
  wrongDecl[1] = Decl::ComponentType::float2;
  EXPECT_FALSE(configureMetalInventoryOfflineReflection(
      Detail::MetalInventoryOfflineRole::Vertex,
      Stage::Vertex,wrongDecl,vertexLayout));

  push.byteSize = 60;
  vertexLayout = {push};
  EXPECT_FALSE(configureMetalInventoryOfflineReflection(
      Detail::MetalInventoryOfflineRole::Vertex,
      Stage::Vertex,vertexDecl,vertexLayout));

  texture.layout = 1;
  fragmentLayout = {texture};
  EXPECT_FALSE(configureMetalInventoryOfflineReflection(
      Detail::MetalInventoryOfflineRole::Fragment,
      Stage::Fragment,{},fragmentLayout));
  }

TEST(MetalBuiltinRuntime,ClassifiesOnlyExactGeneratedSources) {
  expectExactSourceRole(empty_vert_sprv,
                        MetalBuiltinSourceRole::ColorVertex);
  expectExactSourceRole(empty_frag_sprv,
                        MetalBuiltinSourceRole::ColorFragment);
  expectExactSourceRole(tex_brush_vert_sprv,
                        MetalBuiltinSourceRole::TextureVertex);
  expectExactSourceRole(tex_brush_frag_sprv,
                        MetalBuiltinSourceRole::TextureFragment);

  constexpr std::array<uint32_t,5> foreignSpirv = {
      0x07230203,0x00010000,0,1,0,
      };
  EXPECT_EQ(classifyMetalBuiltinSource(
                foreignSpirv.data(),sizeof(foreignSpirv)),
            MetalBuiltinSourceRole::None);
  EXPECT_EQ(classifyMetalBuiltinSource(nullptr,0),
            MetalBuiltinSourceRole::None);
  }

TEST(MetalBuiltinRuntime,ClassifiesAllBuiltinRenderCombinations) {
  const RenderState opaque;
  const RenderState alpha    = alphaBlend();
  const RenderState additive = additiveBlend();

  struct Case final {
    MetalBuiltinSourceRole vertex;
    MetalBuiltinSourceRole fragment;
    Topology topology;
    const RenderState* state;
    MetalBuiltinRenderRole expected;
    };

  const std::array<Case,12> cases = {{
      {MetalBuiltinSourceRole::ColorVertex,
       MetalBuiltinSourceRole::ColorFragment,Topology::Lines,&opaque,
       MetalBuiltinRenderRole::ColorLinesOpaque},
      {MetalBuiltinSourceRole::ColorVertex,
       MetalBuiltinSourceRole::ColorFragment,Topology::Triangles,&opaque,
       MetalBuiltinRenderRole::ColorTrianglesOpaque},
      {MetalBuiltinSourceRole::ColorVertex,
       MetalBuiltinSourceRole::ColorFragment,Topology::Lines,&alpha,
       MetalBuiltinRenderRole::ColorLinesAlpha},
      {MetalBuiltinSourceRole::ColorVertex,
       MetalBuiltinSourceRole::ColorFragment,Topology::Triangles,&alpha,
       MetalBuiltinRenderRole::ColorTrianglesAlpha},
      {MetalBuiltinSourceRole::ColorVertex,
       MetalBuiltinSourceRole::ColorFragment,Topology::Lines,&additive,
       MetalBuiltinRenderRole::ColorLinesAdditive},
      {MetalBuiltinSourceRole::ColorVertex,
       MetalBuiltinSourceRole::ColorFragment,Topology::Triangles,&additive,
       MetalBuiltinRenderRole::ColorTrianglesAdditive},
      {MetalBuiltinSourceRole::TextureVertex,
       MetalBuiltinSourceRole::TextureFragment,Topology::Lines,&opaque,
       MetalBuiltinRenderRole::TextureLinesOpaque},
      {MetalBuiltinSourceRole::TextureVertex,
       MetalBuiltinSourceRole::TextureFragment,Topology::Triangles,&opaque,
       MetalBuiltinRenderRole::TextureTrianglesOpaque},
      {MetalBuiltinSourceRole::TextureVertex,
       MetalBuiltinSourceRole::TextureFragment,Topology::Lines,&alpha,
       MetalBuiltinRenderRole::TextureLinesAlpha},
      {MetalBuiltinSourceRole::TextureVertex,
       MetalBuiltinSourceRole::TextureFragment,Topology::Triangles,&alpha,
       MetalBuiltinRenderRole::TextureTrianglesAlpha},
      {MetalBuiltinSourceRole::TextureVertex,
       MetalBuiltinSourceRole::TextureFragment,Topology::Lines,&additive,
       MetalBuiltinRenderRole::TextureLinesAdditive},
      {MetalBuiltinSourceRole::TextureVertex,
       MetalBuiltinSourceRole::TextureFragment,Topology::Triangles,&additive,
       MetalBuiltinRenderRole::TextureTrianglesAdditive},
      }};

  for(const auto& testCase:cases) {
    EXPECT_EQ(classifyMetalBuiltinRenderRole(
                  testCase.vertex,testCase.fragment,
                  testCase.topology,*testCase.state),
              testCase.expected);
    }
  }

TEST(MetalBuiltinRuntime,RejectsForeignPipelineInputs) {
  RenderState wrongBlend = alphaBlend();
  wrongBlend.setBlendOp(RenderState::BlendOp::Subtract);

  EXPECT_EQ(classifyMetalBuiltinRenderRole(
                MetalBuiltinSourceRole::ColorVertex,
                MetalBuiltinSourceRole::TextureFragment,
                Topology::Triangles,RenderState()),
            MetalBuiltinRenderRole::None);
  EXPECT_EQ(classifyMetalBuiltinRenderRole(
                MetalBuiltinSourceRole::None,
                MetalBuiltinSourceRole::ColorFragment,
                Topology::Triangles,RenderState()),
            MetalBuiltinRenderRole::None);
  EXPECT_EQ(classifyMetalBuiltinRenderRole(
                MetalBuiltinSourceRole::ColorVertex,
                MetalBuiltinSourceRole::ColorFragment,
                Topology::Points,RenderState()),
            MetalBuiltinRenderRole::None);
  EXPECT_EQ(classifyMetalBuiltinRenderRole(
                MetalBuiltinSourceRole::ColorVertex,
                MetalBuiltinSourceRole::ColorFragment,
                Topology::Triangles,wrongBlend),
            MetalBuiltinRenderRole::None);
  }
