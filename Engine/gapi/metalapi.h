#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <array>
#include <cstddef>
#include <cstdint>

namespace MTL {
class Device;
class Buffer;
class Texture;
class RenderCommandEncoder;
}

namespace Tempest {

class CommandBuffer;
class Device;
class StorageBuffer;
class Texture2d;
class MetalApi;
template<class T>
class Encoder;

// Non-owning Metal view. The pointer is valid only while the originating
// Tempest device or resource remains alive and must never be released.
template<class T>
class BorrowedMetalHandle final {
  public:
    constexpr BorrowedMetalHandle() noexcept = default;

    [[nodiscard]]
    constexpr T* get() const noexcept {
      return value;
      }

    constexpr explicit operator bool() const noexcept {
      return value!=nullptr;
      }

  private:
    constexpr explicit BorrowedMetalHandle(T* value) noexcept
      :value(value) {
      }

    T* value = nullptr;

  friend class MetalApi;
  };

using BorrowedMetalDevice  = BorrowedMetalHandle<MTL::Device>;
using BorrowedMetalBuffer  = BorrowedMetalHandle<MTL::Buffer>;
using BorrowedMetalTexture = BorrowedMetalHandle<MTL::Texture>;
// Invoked synchronously for the active Tempest render pass. The callback must
// neither retain nor release the encoder, end encoding, submit the command
// buffer, nor re-enter Tempest through this command encoder. Tempest pipeline
// and resource caches are invalidated before the callback, on return, and on
// exception, so callers must set a pipeline again before a later Tempest draw.
// The callback must restore or balance every dynamic or debug encoder state
// that it changes, such as viewport, scissor, winding, bias, or debug groups.
using MetalRenderEncodeCallback = void (*)(void*,MTL::RenderCommandEncoder*);

struct MetalRuntimeCompilationSnapshot final {
  bool     available             = false;
  uint64_t sourceLibraryRequests = 0;
  uint64_t computePsoRequests    = 0;
  uint64_t renderPsoRequests     = 0;
  };

enum class MetalBuiltinSourceRole : uint8_t {
  ColorVertex     = 0,
  ColorFragment   = 1,
  TextureVertex   = 2,
  TextureFragment = 3,
  Count           = 4,
  None = 0xFF,
  };

enum class MetalBuiltinRenderRole : uint8_t {
  ColorLinesOpaque          = 0,
  ColorTrianglesOpaque      = 1,
  ColorLinesAlpha           = 2,
  ColorTrianglesAlpha       = 3,
  ColorLinesAdditive        = 4,
  ColorTrianglesAdditive    = 5,
  TextureLinesOpaque        = 6,
  TextureTrianglesOpaque    = 7,
  TextureLinesAlpha         = 8,
  TextureTrianglesAlpha     = 9,
  TextureLinesAdditive      = 10,
  TextureTrianglesAdditive  = 11,
  Count                     = 12,
  None = 0xFF,
  };

[[nodiscard]]
constexpr size_t metalBuiltinSourceRoleIndex(
    MetalBuiltinSourceRole role) noexcept {
  return static_cast<size_t>(role);
  }

[[nodiscard]]
constexpr size_t metalBuiltinRenderRoleIndex(
    MetalBuiltinRenderRole role) noexcept {
  return static_cast<size_t>(role);
  }

struct MetalBuiltinRuntimeSnapshot final {
  bool available = false;
  std::array<uint64_t,
             metalBuiltinSourceRoleIndex(MetalBuiltinSourceRole::Count)>
      sourceLibraryRequests = {};
  std::array<uint64_t,
             metalBuiltinRenderRoleIndex(MetalBuiltinRenderRole::Count)>
      renderPsoRequests = {};
  };

class MetalApi : public AbstractGraphicsApi {
  public:
    explicit MetalApi(ApiFlags f=ApiFlags::NoFlags);
    ~MetalApi();

    std::vector<Props> devices() const override;

    [[nodiscard]]
    static BorrowedMetalDevice  borrowDevice (const Tempest::Device& device) noexcept;
    [[nodiscard]]
    static BorrowedMetalBuffer  borrowBuffer (const Tempest::Device& device,
                                               const StorageBuffer& buffer) noexcept;
    [[nodiscard]]
    static BorrowedMetalTexture borrowTexture(const Tempest::Device& device,
                                               const Texture2d& texture) noexcept;
    [[nodiscard]]
    static MetalRuntimeCompilationSnapshot
        runtimeCompilationSnapshot(const Tempest::Device& device) noexcept;
    [[nodiscard]]
    static MetalBuiltinRuntimeSnapshot
        builtinRuntimeSnapshot(const Tempest::Device& device) noexcept;
    [[nodiscard]]
    static bool withActiveRenderEncoder(
        const Tempest::Device& device,
        Tempest::Encoder<Tempest::CommandBuffer>& encoder,
        void* context,
        MetalRenderEncodeCallback callback);

  protected:
    Device*        createDevice(std::string_view gpuName) override;
    Swapchain*     createSwapchain(SystemApi::Window* w, Device *d) override;

    PPipeline      createPipeline(Device* d, const RenderState &st, Topology tp,
                                  const Shader*const* sh, size_t cnt) override;
    PCompPipeline  createComputePipeline(Device* d, Shader* sh) override;
    PShader        createShader(Device *d, const void* source, size_t src_size) override;

    PBuffer        createBuffer (Device* d, const void *mem, size_t size, MemUsage usage, BufferHeap flg) override;
    PTexture       createTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mips) override;
    PTexture       createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) override;
    PTexture       createStorage(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) override;
    PTexture       createStorage(Device* d, const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mips, TextureFormat frm) override;
#if defined(TEMPEST_METALFX_SPATIAL)
    SpatialScaler* createSpatialScaler(Device* d, const SpatialScalerDesc& desc) override;
#endif
#if defined(TEMPEST_METALFX_TEMPORAL)
    TemporalScaler* createTemporalScaler(Device* d, const TemporalScalerDesc& desc) override;
#endif

    AccelerationStructure* createBottomAccelerationStruct(Device* d, const RtGeometry* geom, size_t size) override;
    AccelerationStructure* createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t size) override;

    DescArray*     createDescriptors(Device* d, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel) override;
    DescArray*     createDescriptors(Device* d, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler& smp) override;
    DescArray*     createDescriptors(Device* d, AbstractGraphicsApi::Buffer**  buf, size_t cnt) override;

    void           readPixels(Device *d, Pixmap &out, const PTexture t,
                              TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) override;
    void           readBytes(Device* d, Buffer* buf, void* out, size_t size) override;

    CommandBuffer* createCommandBuffer(Device* d) override;

    void           present(Device *d, Swapchain* sw) override;
    auto           submit (Device *d, CommandBuffer* cmd) -> std::shared_ptr<AbstractGraphicsApi::Fence> override;

    void           getCaps(Device *d, Props& caps) override;

  private:
    bool           validation = false;
  };

}
