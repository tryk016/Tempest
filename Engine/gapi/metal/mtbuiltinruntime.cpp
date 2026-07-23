#include "mtbuiltinruntime.h"

#include "builtin_shader.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

using namespace Tempest;
using namespace Tempest::Detail;

namespace {

template<size_t N>
bool sourceEquals(const void* source, size_t sourceSize,
                  const uint8_t (&expected)[N]) noexcept {
  return source!=nullptr && sourceSize==N &&
         std::memcmp(source,expected,N)==0;
  }

enum class BlendRole : uint8_t {
  Opaque,
  Alpha,
  Additive,
  None,
  };

bool validManifestString(const char* value) noexcept {
  return value!=nullptr && value[0]!='\0';
  }

bool validManifestSource(const void* source, size_t sourceSize) noexcept {
  return source!=nullptr && sourceSize!=0 && sourceSize%sizeof(uint32_t)==0;
  }

BlendRole classifyBlend(const RenderState& state) noexcept {
  if(state.blendOperation()!=RenderState::BlendOp::Add)
    return BlendRole::None;

  if(state.blendSource()==RenderState::BlendMode::One &&
     state.blendDest()==RenderState::BlendMode::Zero)
    return BlendRole::Opaque;
  if(state.blendSource()==RenderState::BlendMode::SrcAlpha &&
     state.blendDest()==RenderState::BlendMode::OneMinusSrcAlpha)
    return BlendRole::Alpha;
  if(state.blendSource()==RenderState::BlendMode::One &&
     state.blendDest()==RenderState::BlendMode::One)
    return BlendRole::Additive;
  return BlendRole::None;
  }

}

std::shared_ptr<const MetalBuiltinOfflineConfig>
Tempest::Detail::makeMetalBuiltinOfflineConfig(
    const MetalBuiltinOfflineManifest& manifest) {
  if(manifest.abiVersion!=MetalBuiltinOfflineManifest::AbiVersion ||
     manifest.structSize!=MetalBuiltinOfflineManifest::StructSize)
    throw std::invalid_argument("Metal Builtin offline manifest ABI mismatch");

  const std::array<const char*,4> names = {{
      manifest.colorVertexFunction,
      manifest.colorFragmentFunction,
      manifest.textureVertexFunction,
      manifest.textureFragmentFunction,
      }};
  const std::array<const void*,2> inventorySources = {{
      manifest.inventoryVertexSpirv,
      manifest.inventoryFragmentSpirv,
      }};
  const std::array<size_t,2> inventorySourceSizes = {{
      manifest.inventoryVertexSpirvSize,
      manifest.inventoryFragmentSpirvSize,
      }};
  const std::array<const char*,2> inventoryNames = {{
      manifest.inventoryVertexFunction,
      manifest.inventoryFragmentFunction,
      }};
  if(!validManifestString(manifest.metallibPath) ||
     manifest.metallibPath[0]!='/')
    throw std::invalid_argument(
        "Metal Builtin offline metallib path must be absolute");
  for(const char* name:names)
    if(!validManifestString(name))
      throw std::invalid_argument(
          "Metal Builtin offline function name is missing");
  for(size_t i=0; i<names.size(); ++i)
    for(size_t r=0; r<i; ++r)
      if(std::strcmp(names[i],names[r])==0)
        throw std::invalid_argument(
            "Metal Builtin offline function names must be distinct");

  const bool hasInventory =
      inventorySources[0]!=nullptr || inventorySourceSizes[0]!=0 ||
      inventoryNames[0]!=nullptr || inventorySources[1]!=nullptr ||
      inventorySourceSizes[1]!=0 || inventoryNames[1]!=nullptr;
  if(hasInventory) {
    for(size_t i=0; i<inventorySources.size(); ++i) {
      if(!validManifestSource(
             inventorySources[i],inventorySourceSizes[i]) ||
         !validManifestString(inventoryNames[i]))
        throw std::invalid_argument(
            "Metal inventory offline manifest pair is incomplete");
      if(classifyMetalBuiltinSource(
             inventorySources[i],inventorySourceSizes[i])!=
         MetalBuiltinSourceRole::None)
        throw std::invalid_argument(
            "Metal inventory offline source aliases a Tempest Builtin shader");
      for(const char* builtinName:names)
        if(std::strcmp(inventoryNames[i],builtinName)==0)
          throw std::invalid_argument(
              "Metal offline function names must be distinct");
      }
    if(inventorySourceSizes[0]==inventorySourceSizes[1] &&
       std::memcmp(inventorySources[0],inventorySources[1],
                   inventorySourceSizes[0])==0)
      throw std::invalid_argument(
          "Metal inventory offline shader modules must be distinct");
    if(std::strcmp(inventoryNames[0],inventoryNames[1])==0)
      throw std::invalid_argument(
          "Metal offline function names must be distinct");
    }

  auto config = std::make_shared<MetalBuiltinOfflineConfig>();
  config->metallibPath = manifest.metallibPath;
  for(size_t i=0; i<names.size(); ++i)
    config->functionNames[i] = names[i];
  if(hasInventory) {
    for(size_t i=0; i<inventorySources.size(); ++i) {
      const auto* begin =
          static_cast<const uint8_t*>(inventorySources[i]);
      config->inventorySources[i].assign(
          begin,begin+inventorySourceSizes[i]);
      config->inventoryFunctionNames[i] = inventoryNames[i];
      }
    }
  return config;
  }

MetalBuiltinSourceRole Tempest::Detail::classifyMetalBuiltinSource(
    const void* source, size_t sourceSize) noexcept {
  if(sourceEquals(source,sourceSize,empty_vert_sprv))
    return MetalBuiltinSourceRole::ColorVertex;
  if(sourceEquals(source,sourceSize,empty_frag_sprv))
    return MetalBuiltinSourceRole::ColorFragment;
  if(sourceEquals(source,sourceSize,tex_brush_vert_sprv))
    return MetalBuiltinSourceRole::TextureVertex;
  if(sourceEquals(source,sourceSize,tex_brush_frag_sprv))
    return MetalBuiltinSourceRole::TextureFragment;
  return MetalBuiltinSourceRole::None;
  }

MetalInventoryOfflineRole
Tempest::Detail::classifyMetalInventoryOfflineSource(
    const MetalBuiltinOfflineConfig& config,
    const void* source, size_t sourceSize) noexcept {
  if(source==nullptr)
    return MetalInventoryOfflineRole::None;
  for(size_t i=0; i<config.inventorySources.size(); ++i) {
    const auto& expected = config.inventorySources[i];
    if(!expected.empty() && expected.size()==sourceSize &&
       std::memcmp(source,expected.data(),sourceSize)==0)
      return static_cast<MetalInventoryOfflineRole>(i);
    }
  return MetalInventoryOfflineRole::None;
  }

bool Tempest::Detail::configureMetalBuiltinOfflineReflection(
    MetalBuiltinSourceRole role,
    ShaderReflection::Stage stage,
    const std::vector<Decl::ComponentType>& vertexDecl,
    std::vector<ShaderReflection::Binding>& layout) noexcept {
  using Component = Decl::ComponentType;
  using Reflection = ShaderReflection;

  const bool vertex =
      role==MetalBuiltinSourceRole::ColorVertex ||
      role==MetalBuiltinSourceRole::TextureVertex;
  const bool fragment =
      role==MetalBuiltinSourceRole::ColorFragment ||
      role==MetalBuiltinSourceRole::TextureFragment;
  if(!vertex && !fragment)
    return false;

  const std::array<Component,3> expectedDecl = {{
      Component::float3,
      Component::float2,
      Component::float4,
      }};
  if(vertex) {
    if(stage!=Reflection::Stage::Vertex ||
       vertexDecl.size()!=expectedDecl.size() ||
       !std::equal(vertexDecl.begin(),vertexDecl.end(),
                   expectedDecl.begin()))
      return false;
    }
  else if(stage!=Reflection::Stage::Fragment || !vertexDecl.empty()) {
    return false;
    }

  if(role!=MetalBuiltinSourceRole::TextureFragment)
    return layout.empty();
  if(layout.size()!=1)
    return false;

  auto& binding = layout[0];
  if(binding.layout!=0 ||
     binding.cls!=Reflection::Class::Texture ||
     binding.stage!=Reflection::Stage::Fragment ||
     binding.runtimeSized ||
     binding.is3DImage ||
     binding.arraySize!=1 ||
     binding.byteSize!=0 ||
     binding.varByteSize!=0)
    return false;

  binding.mslBinding  = 0;
  binding.mslBinding2 = 0;
  binding.mslSize     = 0;
  return true;
  }

bool Tempest::Detail::configureMetalInventoryOfflineReflection(
    MetalInventoryOfflineRole role,
    ShaderReflection::Stage stage,
    const std::vector<Decl::ComponentType>& vertexDecl,
    std::vector<ShaderReflection::Binding>& layout) noexcept {
  using Component = Decl::ComponentType;
  using Reflection = ShaderReflection;

  if(role==MetalInventoryOfflineRole::Vertex) {
    const std::array<Component,4> expectedDecl = {{
        Component::float3,
        Component::float3,
        Component::float2,
        Component::uint1,
        }};
    if(stage!=Reflection::Stage::Vertex ||
       vertexDecl.size()!=expectedDecl.size() ||
       !std::equal(vertexDecl.begin(),vertexDecl.end(),
                   expectedDecl.begin()) ||
       layout.size()!=1)
      return false;

    auto& push = layout[0];
    if(push.layout!=0 ||
       push.cls!=Reflection::Class::Push ||
       push.stage!=Reflection::Stage::Vertex ||
       push.runtimeSized ||
       push.is3DImage ||
       push.arraySize!=0 ||
       push.byteSize!=64 ||
       push.varByteSize!=0)
      return false;
    push.mslBinding  = 0;
    push.mslBinding2 = uint32_t(-1);
    push.mslSize     = 64;
    return true;
    }

  if(role!=MetalInventoryOfflineRole::Fragment ||
     stage!=Reflection::Stage::Fragment ||
     !vertexDecl.empty() ||
     layout.size()!=1)
    return false;

  auto& texture = layout[0];
  if(texture.layout!=0 ||
     texture.cls!=Reflection::Class::Texture ||
     texture.stage!=Reflection::Stage::Fragment ||
     texture.runtimeSized ||
     texture.is3DImage ||
     texture.arraySize!=1 ||
     texture.byteSize!=0 ||
     texture.varByteSize!=0)
    return false;
  texture.mslBinding  = 0;
  texture.mslBinding2 = 0;
  texture.mslSize     = 0;
  return true;
  }

MetalBuiltinRenderRole Tempest::Detail::classifyMetalBuiltinRenderRole(
    MetalBuiltinSourceRole vertex,
    MetalBuiltinSourceRole fragment,
    Topology topology,
    const RenderState& renderState) noexcept {
  const bool color =
      vertex==MetalBuiltinSourceRole::ColorVertex &&
      fragment==MetalBuiltinSourceRole::ColorFragment;
  const bool texture =
      vertex==MetalBuiltinSourceRole::TextureVertex &&
      fragment==MetalBuiltinSourceRole::TextureFragment;
  if(!color && !texture)
    return MetalBuiltinRenderRole::None;

  const bool lines = topology==Topology::Lines;
  if(!lines && topology!=Topology::Triangles)
    return MetalBuiltinRenderRole::None;

  switch(classifyBlend(renderState)) {
    case BlendRole::Opaque:
      if(color)
        return lines ? MetalBuiltinRenderRole::ColorLinesOpaque
                     : MetalBuiltinRenderRole::ColorTrianglesOpaque;
      return lines ? MetalBuiltinRenderRole::TextureLinesOpaque
                   : MetalBuiltinRenderRole::TextureTrianglesOpaque;
    case BlendRole::Alpha:
      if(color)
        return lines ? MetalBuiltinRenderRole::ColorLinesAlpha
                     : MetalBuiltinRenderRole::ColorTrianglesAlpha;
      return lines ? MetalBuiltinRenderRole::TextureLinesAlpha
                   : MetalBuiltinRenderRole::TextureTrianglesAlpha;
    case BlendRole::Additive:
      if(color)
        return lines ? MetalBuiltinRenderRole::ColorLinesAdditive
                     : MetalBuiltinRenderRole::ColorTrianglesAdditive;
      return lines ? MetalBuiltinRenderRole::TextureLinesAdditive
                   : MetalBuiltinRenderRole::TextureTrianglesAdditive;
    case BlendRole::None:
      return MetalBuiltinRenderRole::None;
    }
  return MetalBuiltinRenderRole::None;
  }

bool Tempest::Detail::isMetalInventoryPipelineArchiveEligible(
    MetalInventoryOfflineRole vertex,
    MetalInventoryOfflineRole fragment,
    Topology topology,
    const RenderState& renderState) noexcept {
  return vertex==MetalInventoryOfflineRole::Vertex &&
         fragment==MetalInventoryOfflineRole::Fragment &&
         topology==Topology::Triangles &&
         renderState.cullFaceMode()==RenderState::CullMode::Front &&
         renderState.zTestMode()==RenderState::ZTestMode::LEqual &&
         renderState.isZWriteEnabled() &&
         !renderState.isRasterDiscardEnabled() &&
         renderState.blendSource()==RenderState::BlendMode::One &&
         renderState.blendDest()==RenderState::BlendMode::Zero &&
         renderState.blendOperation()==RenderState::BlendOp::Add;
  }
