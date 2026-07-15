#include "temporalscaler.h"

using namespace Tempest;

TemporalScaler::~TemporalScaler() {
  delete impl.handler;
  }

TemporalScaler& TemporalScaler::operator=(TemporalScaler&& other) {
  std::swap(impl,other.impl);
  return *this;
  }
