#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
template<class T>
class Encoder;

class TemporalScaler final {
  public:
    TemporalScaler() = default;
    TemporalScaler(TemporalScaler&&) = default;
    ~TemporalScaler();

    TemporalScaler& operator=(TemporalScaler&& other);

    bool isEmpty() const { return impl.handler==nullptr; }

  private:
    explicit TemporalScaler(AbstractGraphicsApi::TemporalScaler* scaler):impl(scaler) {}

    Detail::DPtr<AbstractGraphicsApi::TemporalScaler*> impl;

  friend class Tempest::Device;
  friend class Encoder<Tempest::CommandBuffer>;
  };

}
