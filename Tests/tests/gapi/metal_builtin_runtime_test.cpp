#include "../../../Engine/gapi/metal/mtbuiltinruntime.h"

#include "builtin_shader.h"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <vector>

using namespace Tempest;

namespace {

using Tempest::Detail::classifyMetalBuiltinRenderRole;
using Tempest::Detail::classifyMetalBuiltinSource;

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
