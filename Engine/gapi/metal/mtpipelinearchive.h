#pragma once

#include <Tempest/MetalApi>

#include <memory>
#include <string>

namespace MTL {
class Device;
class RenderPipelineDescriptor;
class RenderPipelineState;
}

namespace NS {
class Error;
}

namespace Tempest {
namespace Detail {

class MtDevice;

struct MetalPipelineArchiveConfigOwned final {
  std::string archivePath;
  };

[[nodiscard]]
std::shared_ptr<const MetalPipelineArchiveConfigOwned>
makeMetalPipelineArchiveConfig(
    const MetalPipelineArchiveConfig& config);

[[nodiscard]]
constexpr bool isMetalPipelineArchiveRenderRole(
    MetalBuiltinRenderRole role) noexcept {
  return role==MetalBuiltinRenderRole::ColorTrianglesAlpha ||
         role==MetalBuiltinRenderRole::TextureTrianglesOpaque ||
         role==MetalBuiltinRenderRole::TextureTrianglesAlpha;
  }

class MtPipelineArchive final {
  public:
    MtPipelineArchive(
        MTL::Device& device,
        std::shared_ptr<const MetalPipelineArchiveConfigOwned> config);
    ~MtPipelineArchive();

    MtPipelineArchive(const MtPipelineArchive&) = delete;
    MtPipelineArchive& operator=(const MtPipelineArchive&) = delete;

    [[nodiscard]]
    MTL::RenderPipelineState* newRenderPipelineState(
        MtDevice& device,
        MTL::RenderPipelineDescriptor& descriptor,
        MetalBuiltinRenderRole role,
        bool inventoryArchiveEligible,
        NS::Error** error);

    [[nodiscard]]
    MetalPipelineArchiveSnapshot snapshot() const noexcept;

    [[nodiscard]]
    bool flush() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
  };

}
}
