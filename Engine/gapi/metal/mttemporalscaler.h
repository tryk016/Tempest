#pragma once

#if defined(TEMPEST_BUILD_METAL) && defined(TEMPEST_METALFX_TEMPORAL)

#include <Tempest/AbstractGraphicsApi>
#include <MetalFX/MetalFX.hpp>

#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;
class MtTexture;

class MtTemporalScaler final : public AbstractGraphicsApi::TemporalScaler {
  public:
    MtTemporalScaler(MtDevice& device, const TemporalScalerDesc& desc);

    bool isValid() const { return impl!=nullptr; }
    bool encode(MTL::CommandBuffer& cmd, MtTexture& color, MtTexture& depth,
                MtTexture& motion, MtTexture& output, const TemporalScalerArgs& args);

  private:
    NsPtr<MTLFX::TemporalScaler> impl;
  };

}
}

#endif
