#if defined(TEMPEST_BUILD_VULKAN)

#include "vsetlayoutcache.h"
#include "gapi/vulkan/vdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

static bool compare(const ShaderReflection::LayoutDesc& a, const ShaderReflection::LayoutDesc& b) {
  if(std::memcmp(a.bindings, b.bindings, sizeof(a.bindings))!=0)
    return false;
  if(std::memcmp(a.stage, b.stage, sizeof(a.stage))!=0)
    return false;
  if(std::memcmp(a.count, b.count, sizeof(a.count))!=0)
    return false;
  if(a.runtime!=b.runtime || a.array!=b.array || a.active!=b.active)
    return false;
  return true;
  }

VSetLayoutCache::VSetLayoutCache(VDevice& dev)
  :dev(dev) {
  }

VSetLayoutCache::~VSetLayoutCache() {
  for(auto& i:layouts)
    vkDestroyDescriptorSetLayout(dev.device.impl, i.lay, nullptr);
  }

VkDescriptorSetLayout VSetLayoutCache::findLayout(const ShaderReflection::LayoutDesc& l) {
  if(l.runtime!=0 &&
     !dev.props.descriptors.nonUniformIndexing &&
     !dev.props.hasDescriptorHeap) {
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension,
                            "descriptor indexing");
    }

  std::lock_guard<std::mutex> guard(syncLay);
  for(auto& i:layouts) {
    if(!compare(i.desc,l))
      continue;
    return i.lay;
    }

  Layout& ret = layouts.emplace_back();
  ret.desc = l;

  VkDescriptorSetLayoutBinding bind[MaxBindings] = {};
  VkDescriptorBindingFlags     flg [MaxBindings] = {};
  uint32_t                     count             = 0;
  for(size_t i=0; i<MaxBindings; ++i) {
    if(l.bindings[i]==ShaderReflection::Count)
      continue;
    if((l.active & (1u << i))==0)
      continue;

    auto& b = bind[count];
    if((l.runtime & (1u << i))!=0)
      flg[count] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

    b.binding         = uint32_t(i);
    b.descriptorCount = l.count[i];
    b.descriptorCount = std::max<uint32_t>(1, b.descriptorCount); // WA for VUID-VkGraphicsPipelineCreateInfo-layout-07988
    b.descriptorType  = nativeFormat(l.bindings[i]);
    b.stageFlags      = nativeFormat(l.stage[i]);
    ++count;
    }

  VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags = {};
  bindingFlags.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  bindingFlags.bindingCount  = count;
  bindingFlags.pBindingFlags = flg;

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.pNext        = nullptr;
  info.flags        = 0;
  info.bindingCount = count;
  info.pBindings    = bind;

  if(l.runtime!=0) {
    info.pNext  = &bindingFlags;
    }

  try {
    vkAssert(vkCreateDescriptorSetLayout(dev.device.impl, &info, nullptr, &ret.lay));
    }
  catch(...) {
    layouts.pop_back();
    throw;
    }
  return ret.lay;
  }

#endif
