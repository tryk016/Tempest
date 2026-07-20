#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>

namespace MTL {
class Device;
class Buffer;
class Texture;
class RenderCommandEncoder;
}

namespace Tempest {

namespace Detail {
struct MetalBuiltinOfflineConfig;
struct MetalPipelineArchiveConfigOwned;
}

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

// Versioned C-compatible configuration for the device-wide Metal binary
// archive. MetalApi copies the UTF-8 path. The path must be absolute.
struct MetalPipelineArchiveConfig final {
  static constexpr uint32_t AbiVersion = 1;
  static constexpr uint32_t StructSize =
      2*sizeof(uint32_t) + sizeof(const char*);

  uint32_t    abiVersion = AbiVersion;
  uint32_t    structSize = StructSize;
  const char* archivePath = nullptr;
  };

// Trivially-copyable diagnostic view of the binary archive. Counter values
// are monotonic for the lifetime of the originating Tempest device.
struct MetalPipelineArchiveSnapshot final {
  static constexpr uint32_t AbiVersion = 1;
  static constexpr uint32_t StructSize =
      4*sizeof(uint32_t) + 13*sizeof(uint64_t);

  static constexpr uint32_t Configured         = 1u << 0;
  static constexpr uint32_t Available          = 1u << 1;
  static constexpr uint32_t LoadedFromDisk     = 1u << 2;
  static constexpr uint32_t CreatedEmpty       = 1u << 3;
  static constexpr uint32_t Dirty              = 1u << 4;
  static constexpr uint32_t DisabledAfterError = 1u << 5;

  uint32_t abiVersion = AbiVersion;
  uint32_t structSize = StructSize;
  uint32_t flags      = 0;
  uint32_t reserved   = 0;

  uint64_t loadFailures = 0;
  uint64_t rebuilds     = 0;

  uint64_t renderHits      = 0;
  uint64_t renderMisses    = 0;
  uint64_t renderAdds      = 0;
  uint64_t renderFallbacks = 0;

  uint64_t computeHits      = 0;
  uint64_t computeMisses    = 0;
  uint64_t computeAdds      = 0;
  uint64_t computeFallbacks = 0;

  uint64_t flushAttempts  = 0;
  uint64_t flushSuccesses = 0;
  uint64_t flushFailures  = 0;
  };

static_assert(std::is_standard_layout_v<MetalPipelineArchiveConfig>);
static_assert(std::is_trivially_copyable_v<MetalPipelineArchiveConfig>);
static_assert(sizeof(MetalPipelineArchiveConfig)==
              MetalPipelineArchiveConfig::StructSize);
static_assert(std::is_standard_layout_v<MetalPipelineArchiveSnapshot>);
static_assert(std::is_trivially_copyable_v<MetalPipelineArchiveSnapshot>);
static_assert(sizeof(MetalPipelineArchiveSnapshot)==
              MetalPipelineArchiveSnapshot::StructSize);

// Versioned C-compatible view of the four Tempest Builtin shader entry points
// and the optional OpenGothic inventory pair in an offline Metal library.
// MetalApi copies all strings and optional SPIR-V blobs. The path is UTF-8 and
// must be absolute. The inventory pair is fail-closed: either all six fields
// are null/zero, or all six must describe two distinct exact shader modules.
struct MetalBuiltinOfflineManifest final {
  static constexpr uint32_t AbiVersion = 2;
  static constexpr uint32_t StructSize =
      2*sizeof(uint32_t) + 9*sizeof(const void*) + 2*sizeof(size_t);

  uint32_t    abiVersion = AbiVersion;
  uint32_t    structSize = StructSize;
  const char* metallibPath = nullptr;
  const char* colorVertexFunction = nullptr;
  const char* colorFragmentFunction = nullptr;
  const char* textureVertexFunction = nullptr;
  const char* textureFragmentFunction = nullptr;
  const void* inventoryVertexSpirv = nullptr;
  size_t      inventoryVertexSpirvSize = 0;
  const char* inventoryVertexFunction = nullptr;
  const void* inventoryFragmentSpirv = nullptr;
  size_t      inventoryFragmentSpirvSize = 0;
  const char* inventoryFragmentFunction = nullptr;
  };

class MetalApi : public AbstractGraphicsApi {
  public:
    explicit MetalApi(ApiFlags f=ApiFlags::NoFlags);
    MetalApi(ApiFlags f, const MetalBuiltinOfflineManifest& manifest);
    MetalApi(ApiFlags f,
             const MetalBuiltinOfflineManifest& manifest,
             const MetalPipelineArchiveConfig& archive);
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
    static MetalPipelineArchiveSnapshot
        pipelineArchiveSnapshot(const Tempest::Device& device) noexcept;
    [[nodiscard]]
    static bool flushPipelineArchive(Tempest::Device& device) noexcept;
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
    std::shared_ptr<const Detail::MetalBuiltinOfflineConfig>
                   builtinOffline;
    std::shared_ptr<const Detail::MetalPipelineArchiveConfigOwned>
                   pipelineArchive;
    bool           validation = false;
  };

}
