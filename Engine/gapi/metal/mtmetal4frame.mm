#if defined(TEMPEST_BUILD_METAL) && defined(TEMPEST_METAL4)

#include "mtmetal4frame.h"
#include "mtdevice.h"
#include <Tempest/Except>
#include <algorithm>

#import <Metal/Metal.h>

using namespace Tempest;
using namespace Tempest::Detail;

MtMetal4Queue::MtMetal4Queue(MtDevice& owner) {
  if(@available(macOS 26.0,iOS 26.0,*)) {
    auto device = (id<MTLDevice>)(void*)owner.impl.get();
    @try {
      if([device supportsFamily:MTLGPUFamilyMetal4])
        queue = (void*)[device newMTL4CommandQueue];
      }
    @catch(NSException*) {}
    }
  }

MtMetal4Queue::~MtMetal4Queue() {
  [(id)queue release];
  }

bool MtMetal4Queue::ready() const noexcept { return queue!=nullptr; }

struct MtMetal4Frame::Impl final {
  explicit Impl(MtDevice& owner):device(owner) {
    if(@available(macOS 26.0,iOS 26.0,*)) {
      auto dev = (id<MTLDevice>)(void*)device.impl.get();
      @try {
        allocator = [dev newCommandAllocator];
        command = [dev newCommandBuffer];
        event = [dev newSharedEvent];
        }
      @catch(NSException*) {}
      if(allocator==nil || command==nil || event==nil) {
        [allocator release]; allocator = nil;
        [command release]; command = nil;
        [event release]; event = nil;
        }
      }
    }
  ~Impl() {
    [prefix release]; [allocator release]; [command release]; [event release];
    }
  MtDevice& device;
  id<MTLCommandBuffer> prefix = nil;
  id allocator = nil;
  id command = nil;
  id<MTLSharedEvent> event = nil;
  uint64_t sequence = 0;
  bool ready = false;
  };

MtMetal4Frame::MtMetal4Frame(MtDevice& device):impl(std::make_shared<Impl>(device)) {}
MtMetal4Frame::~MtMetal4Frame() = default;
bool MtMetal4Frame::available() const noexcept {
  return impl->allocator!=nil && impl->command!=nil && impl->event!=nil;
  }
bool MtMetal4Frame::encoded() const noexcept { return impl->ready; }

void MtMetal4Frame::reset() {
  // A submitted frame can outlive or be replaced by its public command wrapper.
  if(impl.use_count()!=1)
    impl = std::make_shared<Impl>(impl->device);
  impl->ready = false;
  [impl->prefix release]; impl->prefix = nil;
  if(@available(macOS 26.0,iOS 26.0,*))
    [impl->allocator reset];
  }

bool MtMetal4Frame::encode(MTL::CommandBuffer* suffix, void* context, Metal4InteropEncodeCallback callback) {
  if(@available(macOS 26.0,iOS 26.0,*)) {
    @try {
      auto queue = (id<MTLCommandQueue>)(void*)impl->device.queue.get();
      auto desc = [MTLCommandBufferDescriptor new];
      desc.retainedReferences = NO;
      desc.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
      impl->prefix = [[queue commandBufferWithDescriptor:desc] retain];
      [desc release];
      if(impl->prefix==nil)
        return false;
      impl->sequence += 2;
      auto after = (id<MTLCommandBuffer>)(void*)suffix;
      [after encodeWaitForEvent:impl->event value:impl->sequence];
      [impl->command beginCommandBufferWithAllocator:impl->allocator];
      const bool success = callback(context,(MTL::CommandBuffer*)(void*)impl->prefix,
                                     (void*)impl->command,suffix);
      [impl->command endCommandBuffer];
      [impl->prefix encodeSignalEvent:impl->event value:impl->sequence-1];
      impl->ready = success && impl->prefix.status==MTLCommandBufferStatusNotEnqueued &&
                              after.status==MTLCommandBufferStatusNotEnqueued;
      return impl->ready;
      }
    @catch(NSException*) { return false; }
    }
  return false;
  }

namespace {
// One fixed three-part frame. Its fence and async token remain pending until
// every child is terminal, including callbacks delayed past a queue event.
struct Completion final {
  Completion(MtDevice& device, std::shared_ptr<MtFence> fence, id<MTLSharedEvent> event,
             std::shared_ptr<void> frameOwner)
    :device(device),fence(std::move(fence)),async(device.asyncState()),event([event retain]),
     frameOwner(std::move(frameOwner)) {}
  ~Completion() { [event release]; [firstError release]; }

  void finish() {
    if(!async->beginCompletion(token))
      return;
    if(!failed && timingKnown)
      fence->gpuSeconds = gpuEnd-gpuStart;
    device.signalFence(*fence,failed ? MTL::CommandBufferStatusError : MTL::CommandBufferStatusCompleted,
                       failed ? MTL::CommandBufferErrorInternal : MTL::CommandBufferErrorNone,
                       (NS::Error*)(void*)firstError);
    async->finishCompletion(token);
    frameOwner.reset();
    }

  void complete(bool success, NSError* error, double start, double end, uint64_t unblock) {
    std::lock_guard<std::mutex> guard(lock);
    failed |= !success;
    if(!success && firstError==nil)
      firstError = [error retain];
    if(start>0 && end>start) {
      gpuStart = gpuStart==0 ? start : std::min(gpuStart,start);
      gpuEnd = std::max(gpuEnd,end);
      }
    else {
      timingKnown = false;
      }
    // A terminal producer error must not strand a submitted dependent buffer.
    // Both error callbacks serialize this monotonic CPU signal under the lock.
    if(!success && unblock>event.signaledValue)
      event.signaledValue = unblock;
    if(--remaining==0)
      finish();
    }

  MtDevice& device;
  std::shared_ptr<MtFence> fence;
  std::shared_ptr<MtAsyncState> async;
  MtAsyncState::SubmissionToken token;
  id<MTLSharedEvent> event;
  std::shared_ptr<void> frameOwner;
  NSError* firstError = nil;
  std::mutex lock;
  unsigned remaining = 3;
  bool failed = false;
  bool timingKnown = true;
  double gpuStart = 0, gpuEnd = 0;
  };
}

void MtMetal4Frame::submit(MTL::CommandBuffer* suffix, const std::shared_ptr<MtFence>& fence) {
  if(@available(macOS 26.0,iOS 26.0,*)) {
    auto state = std::make_shared<Completion>(impl->device,fence,impl->event,impl);
    state->token = state->async->onSubmit();
    if(!state->token) {
      impl->device.signalFence(*fence,MTL::CommandBufferStatusError,MTL::CommandBufferErrorInternal,nullptr);
      throw DeviceLostException("Metal 4 frame rejected after asynchronous failure");
      }
    const uint64_t first = impl->sequence-1, last = impl->sequence;
    auto after = (id<MTLCommandBuffer>)(void*)suffix;
    auto queue = (id<MTL4CommandQueue>)impl->device.metal4Queue->queue;
    bool commitAttempted = false;
    try {
      @try {
        [impl->prefix addCompletedHandler:^(id<MTLCommandBuffer> c) {
          state->complete(c.status==MTLCommandBufferStatusCompleted,c.error,c.GPUStartTime,c.GPUEndTime,first);
          }];
        [after addCompletedHandler:^(id<MTLCommandBuffer> c) {
          state->complete(c.status==MTLCommandBufferStatusCompleted,c.error,c.GPUStartTime,c.GPUEndTime,0);
          }];
        auto options = [[[MTL4CommitOptions alloc] init] autorelease];
        [options addFeedbackHandler:^(id<MTL4CommitFeedback> feedback) {
          state->complete(feedback.error==nil,feedback.error,feedback.GPUStartTime,feedback.GPUEndTime,last);
          }];
        // From this point an exception is an ambiguous submission. Keep the
        // token and owners pending for the existing bounded idle/exit barrier.
        commitAttempted = true;
        [impl->prefix commit];
        [queue waitForEvent:impl->event value:first];
        const id<MTL4CommandBuffer> commands[] = {impl->command};
        [queue commit:commands count:1 options:options];
        [queue signalEvent:impl->event value:last];
        [after commit];
        }
      @catch(NSException*) {
        throw DeviceLostException("Metal 4 frame submission failed");
        }
      }
    catch(...) {
      if(!commitAttempted) {
        state->failed = true;
        state->finish();
        }
      throw;
      }
    return;
    }
  throw DeviceLostException("Metal 4 frame submitted on an unsupported system");
  }

#endif
