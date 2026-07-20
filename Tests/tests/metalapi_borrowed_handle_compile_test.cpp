#include <Tempest/MetalApi>

#include <type_traits>
#include <utility>

namespace {

using Tempest::BorrowedMetalBuffer;
using Tempest::BorrowedMetalDevice;
using Tempest::BorrowedMetalTexture;
using Tempest::MetalBuiltinRenderRole;
using Tempest::MetalBuiltinOfflineManifest;
using Tempest::MetalBuiltinRuntimeSnapshot;
using Tempest::MetalBuiltinSourceRole;
using Tempest::MetalRuntimeCompilationSnapshot;

template<class Handle, class Native>
constexpr bool isBorrowedHandleContract =
    std::is_trivially_copyable_v<Handle> &&
    std::is_trivially_destructible_v<Handle> &&
    std::is_standard_layout_v<Handle> &&
    std::is_default_constructible_v<Handle> &&
    std::is_copy_constructible_v<Handle> &&
    std::is_copy_assignable_v<Handle> &&
    sizeof(Handle)==sizeof(Native*) &&
    alignof(Handle)==alignof(Native*) &&
    std::is_same_v<decltype(std::declval<const Handle&>().get()), Native*> &&
    std::is_constructible_v<bool, Handle> &&
    !std::is_convertible_v<Handle, bool> &&
    !std::is_constructible_v<Handle, Native*>;

static_assert(isBorrowedHandleContract<BorrowedMetalDevice, MTL::Device>);
static_assert(isBorrowedHandleContract<BorrowedMetalBuffer, MTL::Buffer>);
static_assert(isBorrowedHandleContract<BorrowedMetalTexture, MTL::Texture>);

static_assert(!std::is_same_v<BorrowedMetalDevice, BorrowedMetalBuffer>);
static_assert(!std::is_same_v<BorrowedMetalBuffer, BorrowedMetalTexture>);

static_assert(std::is_trivially_copyable_v<MetalRuntimeCompilationSnapshot>);
static_assert(std::is_standard_layout_v<MetalRuntimeCompilationSnapshot>);
static_assert(std::is_default_constructible_v<MetalRuntimeCompilationSnapshot>);
constexpr MetalRuntimeCompilationSnapshot emptySnapshot;
static_assert(!emptySnapshot.available);
static_assert(emptySnapshot.sourceLibraryRequests==0);
static_assert(emptySnapshot.computePsoRequests==0);
static_assert(emptySnapshot.renderPsoRequests==0);

static_assert(std::is_trivially_copyable_v<MetalBuiltinRuntimeSnapshot>);
static_assert(std::is_standard_layout_v<MetalBuiltinRuntimeSnapshot>);
static_assert(std::is_default_constructible_v<MetalBuiltinRuntimeSnapshot>);
constexpr MetalBuiltinRuntimeSnapshot emptyBuiltinSnapshot;
static_assert(!emptyBuiltinSnapshot.available);
static_assert(emptyBuiltinSnapshot.sourceLibraryRequests.size()==4);
static_assert(emptyBuiltinSnapshot.renderPsoRequests.size()==12);
static_assert(emptyBuiltinSnapshot.sourceLibraryRequests[
                  Tempest::metalBuiltinSourceRoleIndex(
                      MetalBuiltinSourceRole::ColorVertex)]==0);
static_assert(emptyBuiltinSnapshot.renderPsoRequests[
                  Tempest::metalBuiltinRenderRoleIndex(
                      MetalBuiltinRenderRole::ColorLinesOpaque)]==0);
static_assert(Tempest::metalBuiltinSourceRoleIndex(
                  MetalBuiltinSourceRole::Count)==4);
static_assert(Tempest::metalBuiltinRenderRoleIndex(
                  MetalBuiltinRenderRole::Count)==12);
static_assert(Tempest::metalBuiltinSourceRoleIndex(
                  MetalBuiltinSourceRole::None)==0xFF);
static_assert(Tempest::metalBuiltinRenderRoleIndex(
                  MetalBuiltinRenderRole::None)==0xFF);

static_assert(std::is_trivially_copyable_v<MetalBuiltinOfflineManifest>);
static_assert(std::is_trivially_destructible_v<MetalBuiltinOfflineManifest>);
static_assert(std::is_standard_layout_v<MetalBuiltinOfflineManifest>);
static_assert(std::is_aggregate_v<MetalBuiltinOfflineManifest>);
static_assert(sizeof(MetalBuiltinOfflineManifest)==
              MetalBuiltinOfflineManifest::StructSize);
constexpr MetalBuiltinOfflineManifest emptyOfflineManifest;
static_assert(emptyOfflineManifest.abiVersion==
              MetalBuiltinOfflineManifest::AbiVersion);
static_assert(emptyOfflineManifest.structSize==
              MetalBuiltinOfflineManifest::StructSize);
static_assert(emptyOfflineManifest.metallibPath==nullptr);
static_assert(emptyOfflineManifest.colorVertexFunction==nullptr);
static_assert(emptyOfflineManifest.colorFragmentFunction==nullptr);
static_assert(emptyOfflineManifest.textureVertexFunction==nullptr);
static_assert(emptyOfflineManifest.textureFragmentFunction==nullptr);
static_assert(std::is_constructible_v<
              Tempest::MetalApi,
              Tempest::ApiFlags,
              const MetalBuiltinOfflineManifest&>);

static_assert(std::is_same_v<
              decltype(Tempest::MetalApi::borrowDevice(
                  std::declval<const Tempest::Device&>())),
              BorrowedMetalDevice>);
static_assert(std::is_same_v<
              decltype(Tempest::MetalApi::borrowBuffer(
                  std::declval<const Tempest::Device&>(),
                  std::declval<const Tempest::StorageBuffer&>())),
              BorrowedMetalBuffer>);
static_assert(std::is_same_v<
              decltype(Tempest::MetalApi::borrowTexture(
                  std::declval<const Tempest::Device&>(),
                  std::declval<const Tempest::Texture2d&>())),
              BorrowedMetalTexture>);

static_assert(noexcept(Tempest::MetalApi::borrowDevice(
    std::declval<const Tempest::Device&>())));
static_assert(noexcept(Tempest::MetalApi::borrowBuffer(
    std::declval<const Tempest::Device&>(),
    std::declval<const Tempest::StorageBuffer&>())));
static_assert(noexcept(Tempest::MetalApi::borrowTexture(
    std::declval<const Tempest::Device&>(),
    std::declval<const Tempest::Texture2d&>())));
static_assert(std::is_same_v<
              decltype(Tempest::MetalApi::runtimeCompilationSnapshot(
                  std::declval<const Tempest::Device&>())),
              MetalRuntimeCompilationSnapshot>);
static_assert(noexcept(Tempest::MetalApi::runtimeCompilationSnapshot(
    std::declval<const Tempest::Device&>())));
static_assert(std::is_same_v<
              decltype(Tempest::MetalApi::builtinRuntimeSnapshot(
                  std::declval<const Tempest::Device&>())),
              MetalBuiltinRuntimeSnapshot>);
static_assert(noexcept(Tempest::MetalApi::builtinRuntimeSnapshot(
    std::declval<const Tempest::Device&>())));

static_assert(std::is_same_v<
              Tempest::MetalRenderEncodeCallback,
              void (*)(void*,MTL::RenderCommandEncoder*)>);
static_assert(std::is_same_v<
              decltype(Tempest::MetalApi::withActiveRenderEncoder(
                  std::declval<const Tempest::Device&>(),
                  std::declval<Tempest::Encoder<Tempest::CommandBuffer>&>(),
                  nullptr,
                  std::declval<Tempest::MetalRenderEncodeCallback>())),
              bool>);
static_assert(!noexcept(Tempest::MetalApi::withActiveRenderEncoder(
    std::declval<const Tempest::Device&>(),
    std::declval<Tempest::Encoder<Tempest::CommandBuffer>&>(),
    nullptr,
    std::declval<Tempest::MetalRenderEncodeCallback>())));

}
