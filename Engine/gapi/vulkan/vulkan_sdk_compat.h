#pragma once

// Android NDK r26 ships Vulkan headers older than VK_EXT_descriptor_heap.
// Tempest keeps the extension runtime-disabled for now, but its fallback
// descriptor-set implementation shares translation units with the newer path.
// Provide the subset of the ratified extension declarations that those files
// need to compile. When the platform headers learn the extension, this shim is
// empty and the official declarations are used instead.
#if defined(__ANDROID__) && !defined(VK_EXT_descriptor_heap)

#define VK_EXT_DESCRIPTOR_HEAP_EXTENSION_NAME "VK_EXT_descriptor_heap"

#define VK_BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT \
  static_cast<VkBufferUsageFlagBits>(0x10000000)
#define VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT \
  static_cast<VkFlags64>(0x1000000000ULL)

#define VK_STRUCTURE_TYPE_IMAGE_DESCRIPTOR_INFO_EXT \
  static_cast<VkStructureType>(1000135001)
#define VK_STRUCTURE_TYPE_RESOURCE_DESCRIPTOR_INFO_EXT \
  static_cast<VkStructureType>(1000135002)
#define VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT \
  static_cast<VkStructureType>(1000135003)
#define VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT \
  static_cast<VkStructureType>(1000135004)
#define VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT \
  static_cast<VkStructureType>(1000135005)
#define VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT \
  static_cast<VkStructureType>(1000135006)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_PROPERTIES_EXT \
  static_cast<VkStructureType>(1000135008)
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_HEAP_FEATURES_EXT \
  static_cast<VkStructureType>(1000135009)

typedef enum VkDescriptorMappingSourceEXT {
  VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_CONSTANT_OFFSET_EXT = 0,
  VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT = 1,
  VK_DESCRIPTOR_MAPPING_SOURCE_MAX_ENUM_EXT = 0x7FFFFFFF
} VkDescriptorMappingSourceEXT;

typedef VkFlags VkSpirvResourceTypeFlagsEXT;
#define VK_SPIRV_RESOURCE_TYPE_SAMPLER_BIT_EXT                    0x00000001
#define VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT              0x00000002
#define VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT            0x00000004
#define VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT           0x00000008
#define VK_SPIRV_RESOURCE_TYPE_COMBINED_SAMPLED_IMAGE_BIT_EXT     0x00000010
#define VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT             0x00000020
#define VK_SPIRV_RESOURCE_TYPE_READ_ONLY_STORAGE_BUFFER_BIT_EXT   0x00000040
#define VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT  0x00000080
#define VK_SPIRV_RESOURCE_TYPE_ACCELERATION_STRUCTURE_BIT_EXT     0x00000100
#define VK_SPIRV_RESOURCE_TYPE_ALL_EXT                            0x7FFFFFFF

typedef struct VkHostAddressRangeEXT {
  void*  address;
  size_t size;
} VkHostAddressRangeEXT;

typedef struct VkHostAddressRangeConstEXT {
  const void* address;
  size_t      size;
} VkHostAddressRangeConstEXT;

typedef struct VkDeviceAddressRangeEXT {
  VkDeviceAddress address;
  VkDeviceSize    size;
} VkDeviceAddressRangeEXT;

typedef struct VkImageDescriptorInfoEXT {
  VkStructureType              sType;
  const void*                  pNext;
  const VkImageViewCreateInfo* pView;
  VkImageLayout                layout;
} VkImageDescriptorInfoEXT;

typedef union VkResourceDescriptorDataEXT {
  const VkImageDescriptorInfoEXT* pImage;
  const void*                     pTexelBuffer;
  const VkDeviceAddressRangeEXT*  pAddressRange;
  const void*                     pTensorARM;
} VkResourceDescriptorDataEXT;

typedef struct VkResourceDescriptorInfoEXT {
  VkStructureType             sType;
  const void*                 pNext;
  VkDescriptorType            type;
  VkResourceDescriptorDataEXT data;
} VkResourceDescriptorInfoEXT;

typedef struct VkBindHeapInfoEXT {
  VkStructureType        sType;
  const void*            pNext;
  VkDeviceAddressRangeEXT heapRange;
  VkDeviceSize           reservedRangeOffset;
  VkDeviceSize           reservedRangeSize;
} VkBindHeapInfoEXT;

typedef struct VkPushDataInfoEXT {
  VkStructureType            sType;
  const void*                pNext;
  uint32_t                   offset;
  VkHostAddressRangeConstEXT data;
} VkPushDataInfoEXT;

typedef struct VkDescriptorMappingSourcePushIndexEXT {
  uint32_t                   heapOffset;
  uint32_t                   pushOffset;
  uint32_t                   heapIndexStride;
  uint32_t                   heapArrayStride;
  const VkSamplerCreateInfo* pEmbeddedSampler;
  VkBool32                   useCombinedImageSamplerIndex;
  uint32_t                   samplerHeapOffset;
  uint32_t                   samplerPushOffset;
  uint32_t                   samplerHeapIndexStride;
  uint32_t                   samplerHeapArrayStride;
} VkDescriptorMappingSourcePushIndexEXT;

typedef union VkDescriptorMappingSourceDataEXT {
  VkDescriptorMappingSourcePushIndexEXT pushIndex;
} VkDescriptorMappingSourceDataEXT;

typedef struct VkDescriptorSetAndBindingMappingEXT {
  VkStructureType                  sType;
  const void*                      pNext;
  uint32_t                         descriptorSet;
  uint32_t                         firstBinding;
  uint32_t                         bindingCount;
  VkSpirvResourceTypeFlagsEXT      resourceMask;
  VkDescriptorMappingSourceEXT     source;
  VkDescriptorMappingSourceDataEXT sourceData;
} VkDescriptorSetAndBindingMappingEXT;

typedef struct VkShaderDescriptorSetAndBindingMappingInfoEXT {
  VkStructureType                         sType;
  const void*                             pNext;
  uint32_t                                mappingCount;
  const VkDescriptorSetAndBindingMappingEXT* pMappings;
} VkShaderDescriptorSetAndBindingMappingInfoEXT;

typedef struct VkPhysicalDeviceDescriptorHeapFeaturesEXT {
  VkStructureType sType;
  void*           pNext;
  VkBool32        descriptorHeap;
  VkBool32        descriptorHeapCaptureReplay;
} VkPhysicalDeviceDescriptorHeapFeaturesEXT;

typedef struct VkPhysicalDeviceDescriptorHeapPropertiesEXT {
  VkStructureType sType;
  void*           pNext;
  VkDeviceSize    samplerHeapAlignment;
  VkDeviceSize    resourceHeapAlignment;
  VkDeviceSize    maxSamplerHeapSize;
  VkDeviceSize    maxResourceHeapSize;
  VkDeviceSize    minSamplerHeapReservedRange;
  VkDeviceSize    minSamplerHeapReservedRangeWithEmbedded;
  VkDeviceSize    minResourceHeapReservedRange;
  VkDeviceSize    samplerDescriptorSize;
  VkDeviceSize    imageDescriptorSize;
  VkDeviceSize    bufferDescriptorSize;
  VkDeviceSize    samplerDescriptorAlignment;
  VkDeviceSize    imageDescriptorAlignment;
  VkDeviceSize    bufferDescriptorAlignment;
  VkDeviceSize    maxPushDataSize;
  size_t          imageCaptureReplayOpaqueDataSize;
  uint32_t        maxDescriptorHeapEmbeddedSamplers;
  uint32_t        samplerYcbcrConversionCount;
  VkBool32        sparseDescriptorHeaps;
  VkBool32        protectedDescriptorHeaps;
} VkPhysicalDeviceDescriptorHeapPropertiesEXT;

typedef VkResult (VKAPI_PTR *PFN_vkWriteSamplerDescriptorsEXT)(
    VkDevice, uint32_t, const VkSamplerCreateInfo*, const VkHostAddressRangeEXT*);
typedef VkResult (VKAPI_PTR *PFN_vkWriteResourceDescriptorsEXT)(
    VkDevice, uint32_t, const VkResourceDescriptorInfoEXT*, const VkHostAddressRangeEXT*);
typedef void (VKAPI_PTR *PFN_vkCmdBindSamplerHeapEXT)(
    VkCommandBuffer, const VkBindHeapInfoEXT*);
typedef void (VKAPI_PTR *PFN_vkCmdBindResourceHeapEXT)(
    VkCommandBuffer, const VkBindHeapInfoEXT*);
typedef void (VKAPI_PTR *PFN_vkCmdPushDataEXT)(
    VkCommandBuffer, const VkPushDataInfoEXT*);

#endif
