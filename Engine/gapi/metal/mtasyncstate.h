#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <condition_variable>
#include <cstdint>
#include <limits>
#include <mutex>
#include <vector>

namespace Tempest {
namespace Detail {

class MtAsyncState final {
  public:
    struct SubmissionToken final {
      static constexpr uint32_t InvalidSlot = std::numeric_limits<uint32_t>::max();

      uint32_t slot       = InvalidSlot;
      uint64_t generation = 0;

      explicit operator bool() const noexcept {
        return slot!=InvalidSlot;
        }
      };

    SubmissionToken onSubmit() {
      std::lock_guard<std::mutex> guard(sync);
      uint32_t slotId = 0;
      for(; slotId<slots.size(); ++slotId)
        if(slots[slotId].state==SlotState::Free)
          break;
      if(slotId==slots.size())
        slots.emplace_back();

      Slot& slot = slots[slotId];
      ++slot.generation;
      if(slot.generation==0)
        ++slot.generation;
      slot.state = SlotState::Active;
      ++inFlight;
      return {slotId,slot.generation};
      }

    bool beginCompletion(SubmissionToken token) noexcept {
      std::lock_guard<std::mutex> guard(sync);
      if(token.slot>=slots.size())
        return false;
      Slot& slot = slots[token.slot];
      if(slot.generation!=token.generation || slot.state!=SlotState::Active)
        return false;
      slot.state = SlotState::Completing;
      return true;
      }

    void finishCompletion(SubmissionToken token,
                          PresentFailure failure = {},
                          bool injectFault = false) noexcept {
      {
        std::lock_guard<std::mutex> guard(sync);
        if(token.slot>=slots.size())
          return;
        Slot& slot = slots[token.slot];
        if(slot.generation!=token.generation || slot.state!=SlotState::Completing)
          return;
        if(injectFault && !presentFaultInjected && !failure) {
          presentFaultInjected = true;
          failure.kind         = PresentFailureKind::DeviceLost;
          failure.nativeCode   = -1;
          }
        if(failure && !presentFailure)
          presentFailure = failure;
        slot.state = SlotState::Free;
        --inFlight;
      }
      idleCv.notify_all();
      }

    void waitIdle() {
      std::unique_lock<std::mutex> guard(sync);
      idleCv.wait(guard,[this](){
        return inFlight==0u;
        });
      }

    PresentFailure takePresentFailure() noexcept {
      std::lock_guard<std::mutex> guard(sync);
      const PresentFailure result = presentFailure;
      presentFailure = {};
      return result;
      }

  private:
    enum class SlotState : uint8_t {
      Free,
      Active,
      Completing,
      };

    struct Slot final {
      uint64_t  generation = 0;
      SlotState state      = SlotState::Free;
      };

    std::condition_variable idleCv;
    std::mutex              sync;
    uint32_t                inFlight = 0;
    std::vector<Slot>       slots;
    PresentFailure          presentFailure;
    bool                    presentFaultInjected = false;
  };

}
}
