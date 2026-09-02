#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VTimestampPool final : public AbstractGraphicsApi::TimestampPool {
  public:
    VTimestampPool(VDevice& device, uint32_t count);
    ~VTimestampPool() override;

    uint32_t size() const override;
    bool     tryRead(uint32_t query, uint64_t& value) const override;
    uint64_t elapsedNs(uint64_t begin, uint64_t end) const override;

    VkQueryPool impl = VK_NULL_HANDLE;

  private:
    VDevice&   device;
    uint32_t   queryCount      = 0;
    uint64_t   timestampMask   = 0;
    float      timestampPeriod = 0.f;
  };

}}
