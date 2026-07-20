#include "mtbuiltinruntime.h"

#include "builtin_shader.h"

#include <cstring>

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
