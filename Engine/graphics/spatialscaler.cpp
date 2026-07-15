#include "spatialscaler.h"

using namespace Tempest;

SpatialScaler::~SpatialScaler() {
  delete impl.handler;
  }

SpatialScaler& SpatialScaler::operator=(SpatialScaler&& other) {
  std::swap(impl, other.impl);
  return *this;
  }
