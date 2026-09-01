#pragma once

#if defined(TEMPEST_BUILD_METALFX)

#include <Tempest/AbstractGraphicsApi>
#include <MetalFX/MetalFX.hpp>

#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;
class MtTexture;

class MtSpatialScaler final : public AbstractGraphicsApi::SpatialScaler {
  public:
    MtSpatialScaler(MtDevice& device, const SpatialScalerDesc& desc);

    bool isValid() const { return impl!=nullptr; }
    bool belongsTo(const MtDevice& device) const { return owner==&device; }
    bool encode(MTL::CommandBuffer& cmd, MtTexture& input, MtTexture& output);

  private:
    MtDevice*                   owner = nullptr;
    NsPtr<MTLFX::SpatialScaler> impl;
  };

}
}

#endif
