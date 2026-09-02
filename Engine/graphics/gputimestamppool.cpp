#include "gputimestamppool.h"

#include <stdexcept>
#include <utility>

using namespace Tempest;

GpuTimestampPool::GpuTimestampPool(AbstractGraphicsApi::PTimestampPool&& impl)
  :impl(std::move(impl)) {
  }

uint32_t GpuTimestampPool::size() const {
  return impl.handler!=nullptr ? impl.handler->size() : 0;
  }

bool GpuTimestampPool::tryRead(uint32_t query, uint64_t& value) const {
  if(impl.handler==nullptr)
    return false;
  if(query>=impl.handler->size())
    throw std::out_of_range("timestamp query index");
  return impl.handler->tryRead(query,value);
  }

bool GpuTimestampPool::elapsedNs(uint32_t beginQuery, uint32_t endQuery, uint64_t& value) const {
  if(impl.handler==nullptr)
    return false;
  if(beginQuery>=impl.handler->size() || endQuery>=impl.handler->size())
    throw std::out_of_range("timestamp query index");

  uint64_t begin = 0;
  uint64_t end   = 0;
  if(!impl.handler->tryRead(beginQuery,begin) || !impl.handler->tryRead(endQuery,end))
    return false;
  value = impl.handler->elapsedNs(begin,end);
  return true;
  }
