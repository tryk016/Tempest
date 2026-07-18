#include <Tempest/MetalApi>

#include <type_traits>
#include <utility>

namespace {

using Tempest::BorrowedMetalBuffer;
using Tempest::BorrowedMetalDevice;
using Tempest::BorrowedMetalTexture;

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
