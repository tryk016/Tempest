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

  auto config = std::make_shared<MetalBuiltinOfflineConfig>();
  config->metallibPath = manifest.metallibPath;
  for(size_t i=0; i<names.size(); ++i)
    config->functionNames[i] = names[i];
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
