#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class Device;
class CommandBuffer;

template<class T>
class Encoder;

class GpuTimestampPool final {
  public:
    GpuTimestampPool() = default;
    GpuTimestampPool(GpuTimestampPool&&) = default;
    ~GpuTimestampPool() = default;
    GpuTimestampPool& operator=(GpuTimestampPool&&) = default;

    bool     isEmpty() const { return impl.handler==nullptr; }
    uint32_t size() const;

    // Reads the backend timestamp counter without waiting for the GPU.
    bool tryRead(uint32_t query, uint64_t& value) const;
    // Reads both counters and converts their wrap-safe difference to nanoseconds.
    bool elapsedNs(uint32_t beginQuery, uint32_t endQuery, uint64_t& value) const;

  private:
    explicit GpuTimestampPool(AbstractGraphicsApi::PTimestampPool&& impl);

    AbstractGraphicsApi::PTimestampPool impl;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

}
