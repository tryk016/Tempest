#if defined(TEMPEST_BUILD_VULKAN)

#include "vtimestamppool.h"

#include "vdevice.h"

#include <limits>

using namespace Tempest;
using namespace Tempest::Detail;

VTimestampPool::VTimestampPool(VDevice& device, uint32_t count)
  :device(device), queryCount(count), timestampPeriod(device.props.timestampPeriod) {
  const uint32_t validBits = device.props.timestampValidBits;
  timestampMask = validBits<64 ? (uint64_t(1)<<validBits)-1 : std::numeric_limits<uint64_t>::max();

  VkQueryPoolCreateInfo info = {};
  info.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  info.queryType  = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = count;
  vkAssert(vkCreateQueryPool(device.device.impl,&info,nullptr,&impl));
  }

VTimestampPool::~VTimestampPool() {
  vkDestroyQueryPool(device.device.impl,impl,nullptr);
  }

uint32_t VTimestampPool::size() const {
  return queryCount;
  }

bool VTimestampPool::tryRead(uint32_t query, uint64_t& value) const {
  struct QueryResult {
    uint64_t value        = 0;
    uint64_t availability = 0;
    } result;

  const VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WITH_AVAILABILITY_BIT;
  const VkResult ret = vkGetQueryPoolResults(device.device.impl,impl,query,1,
                                              sizeof(result),&result,sizeof(result),flags);
  if(ret==VK_NOT_READY)
    return false;
  vkAssert(ret);
  if(result.availability==0)
    return false;
  value = result.value & timestampMask;
  return true;
  }

uint64_t VTimestampPool::elapsedNs(uint64_t begin, uint64_t end) const {
  const uint64_t elapsed = (end-begin) & timestampMask;
  const long double ns = static_cast<long double>(elapsed)*timestampPeriod;
  if(ns>=static_cast<long double>(std::numeric_limits<uint64_t>::max()))
    return std::numeric_limits<uint64_t>::max();
  return static_cast<uint64_t>(ns);
  }

#endif
