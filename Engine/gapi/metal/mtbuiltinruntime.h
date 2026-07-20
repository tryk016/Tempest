#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/MetalApi>
#include <Tempest/RenderState>

#include <cstddef>

namespace Tempest {
namespace Detail {

[[nodiscard]]
MetalBuiltinSourceRole classifyMetalBuiltinSource(
    const void* source, size_t sourceSize) noexcept;

[[nodiscard]]
MetalBuiltinRenderRole classifyMetalBuiltinRenderRole(
    MetalBuiltinSourceRole vertex,
    MetalBuiltinSourceRole fragment,
    Topology topology,
    const RenderState& renderState) noexcept;

}
}
