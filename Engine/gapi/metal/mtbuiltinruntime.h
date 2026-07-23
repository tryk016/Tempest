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

enum class MetalInventoryOfflineRole : uint8_t {
  Vertex   = 0,
  Fragment = 1,
  Count    = 2,
  None     = 0xFF,
  };

[[nodiscard]]
constexpr size_t metalInventoryOfflineRoleIndex(
    MetalInventoryOfflineRole role) noexcept {
  return static_cast<size_t>(role);
  }

struct MetalBuiltinOfflineConfig final {
  std::string metallibPath;
  std::array<std::string,
             metalBuiltinSourceRoleIndex(MetalBuiltinSourceRole::Count)>
      functionNames;
  std::array<std::vector<uint8_t>,
             metalInventoryOfflineRoleIndex(
                 MetalInventoryOfflineRole::Count)>
      inventorySources;
  std::array<std::string,
             metalInventoryOfflineRoleIndex(
                 MetalInventoryOfflineRole::Count)>
      inventoryFunctionNames;
  };

[[nodiscard]]
std::shared_ptr<const MetalBuiltinOfflineConfig>
makeMetalBuiltinOfflineConfig(
    const MetalBuiltinOfflineManifest& manifest);

[[nodiscard]]
MetalBuiltinSourceRole classifyMetalBuiltinSource(
    const void* source, size_t sourceSize) noexcept;

[[nodiscard]]
MetalInventoryOfflineRole classifyMetalInventoryOfflineSource(
    const MetalBuiltinOfflineConfig& config,
    const void* source, size_t sourceSize) noexcept;

[[nodiscard]]
bool configureMetalBuiltinOfflineReflection(
    MetalBuiltinSourceRole role,
    ShaderReflection::Stage stage,
    const std::vector<Decl::ComponentType>& vertexDecl,
    std::vector<ShaderReflection::Binding>& layout) noexcept;

[[nodiscard]]
bool configureMetalInventoryOfflineReflection(
    MetalInventoryOfflineRole role,
    ShaderReflection::Stage stage,
    const std::vector<Decl::ComponentType>& vertexDecl,
    std::vector<ShaderReflection::Binding>& layout) noexcept;

[[nodiscard]]
MetalBuiltinRenderRole classifyMetalBuiltinRenderRole(
    MetalBuiltinSourceRole vertex,
    MetalBuiltinSourceRole fragment,
    Topology topology,
    const RenderState& renderState) noexcept;

[[nodiscard]]
bool isMetalInventoryPipelineArchiveEligible(
    MetalInventoryOfflineRole vertex,
    MetalInventoryOfflineRole fragment,
    Topology topology,
    const RenderState& renderState) noexcept;

}
}
