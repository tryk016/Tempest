#pragma once

#include <Tempest/MetalApi>
#include <memory>

namespace Tempest::Detail {
class MtDevice;
struct MtFence;

class MtMetal4Queue final {
  public:
    explicit MtMetal4Queue(MtDevice& device);
    ~MtMetal4Queue();
    bool ready() const noexcept;
  private:
    void* queue = nullptr;
    friend class MtMetal4Frame;
  };

class MtMetal4Frame final {
  public:
    explicit MtMetal4Frame(MtDevice& device);
    ~MtMetal4Frame();
    bool available() const noexcept;
    bool encoded() const noexcept;
    bool encode(MTL::CommandBuffer* suffix, void* context, Metal4InteropEncodeCallback callback);
    void submit(MTL::CommandBuffer* suffix, const std::shared_ptr<MtFence>& fence);
    // The caller has confirmed aggregate completion, idle, or no submission.
    void reset();
  private:
    struct Impl;
    std::shared_ptr<Impl> impl;
  };
}
