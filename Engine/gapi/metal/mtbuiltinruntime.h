#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/MetalApi>
#include <Tempest/RenderState>

#include "../shaderreflection.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace Tempest {
namespace Detail {

struct MetalBuiltinOfflineConfig final {
  std::string metallibPath;
  std::array<std::string,
             metalBuiltinSourceRoleIndex(MetalBuiltinSourceRole::Count)>
      functionNames;
  };

[[nodiscard]]
std::shared_ptr<const MetalBuiltinOfflineConfig>
makeMetalBuiltinOfflineConfig(
    const MetalBuiltinOfflineManifest& manifest);

[[nodiscard]]
MetalBuiltinSourceRole classifyMetalBuiltinSource(
    const void* source, size_t sourceSize) noexcept;

[[nodiscard]]
bool configureMetalBuiltinOfflineReflection(
    MetalBuiltinSourceRole role,
    ShaderReflection::Stage stage,
    const std::vector<Decl::ComponentType>& vertexDecl,
    std::vector<ShaderReflection::Binding>& layout) noexcept;

[[nodiscard]]
MetalBuiltinRenderRole classifyMetalBuiltinRenderRole(
    MetalBuiltinSourceRole vertex,
    MetalBuiltinSourceRole fragment,
    Topology topology,
    const RenderState& renderState) noexcept;

}
}
