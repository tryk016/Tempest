#include <Tempest/Device>
#include <Tempest/Encoder>
#include <Tempest/Fence>
#include <Tempest/MetalApi>
#include <gtest/gtest.h>
#include <cstdlib>
#include <Tempest/Except>
#include "gapi/metal/mtdevice.h"
#include "gapi/metal/mtmetal4frame.h"
#import <Metal/Metal.h>

namespace {
struct CopyFrame {
  id<MTLBuffer> source, middle, output;
  id<MTLResidencySet> residency;
  uint8_t value;
  bool reject = false;
  id<MTLSharedEvent> hold = nil;
  };

bool encodeCopy(void* opaque, MTL::CommandBuffer* before, void* body, MTL::CommandBuffer* after) {
  if(@available(macOS 26.0,*)) {
    auto& frame = *static_cast<CopyFrame*>(opaque);
    if(frame.reject)
      return false;
    auto prefix = (id<MTLCommandBuffer>)(void*)before;
    auto metal4 = (id<MTL4CommandBuffer>)body;
    auto suffix = (id<MTLCommandBuffer>)(void*)after;
    if(frame.hold!=nil)
      [prefix encodeWaitForEvent:frame.hold value:1];
    auto fill = [prefix blitCommandEncoder];
    [fill fillBuffer:frame.source range:NSMakeRange(0,64) value:frame.value];
    [fill endEncoding];
    [metal4 useResidencySet:frame.residency];
    auto copy = [metal4 computeCommandEncoder];
    [copy copyFromBuffer:frame.source sourceOffset:0 toBuffer:frame.middle destinationOffset:0 size:64];
    [copy endEncoding];
    auto readback = [suffix blitCommandEncoder];
    [readback copyFromBuffer:frame.middle sourceOffset:0 toBuffer:frame.output destinationOffset:0 size:64];
    [readback endEncoding];
    return true;
    }
  return false;
  }
}

TEST(MetalApi,Metal4FrameInterop) {
  @autoreleasepool {
    if(@available(macOS 26.0,*)) {
      Tempest::MetalApi api{Tempest::ApiFlags::Validation};
      Tempest::Device device(api);
      auto native = (id<MTLDevice>)(void*)Tempest::MetalApi::borrowDevice(device).get();
      ASSERT_TRUE([native supportsFamily:MTLGPUFamilyMetal4]);
      auto source = [native newBufferWithLength:64 options:MTLResourceStorageModePrivate];
      auto middle = [native newBufferWithLength:64 options:MTLResourceStorageModePrivate];
      auto output = [native newBufferWithLength:64 options:MTLResourceStorageModeShared];
      auto descriptor = [MTLResidencySetDescriptor new];
      descriptor.initialCapacity = 3;
      auto residency = [native newResidencySetWithDescriptor:descriptor error:nil];
      id<MTLAllocation> allocations[] = {source,middle,output};
      [residency addAllocations:allocations count:3]; [residency commit];
      CopyFrame frame{source,middle,output,residency,0};
      auto command = device.commandBuffer();
      for(uint8_t value:{41,83,167}) {
        frame.value = value;
        if(value==167)
          frame.hold = [native newSharedEvent];
        {
          auto encoder = command.startEncoding(device);
          ASSERT_EQ(Tempest::MetalApi::stageMetal4Interop(device,encoder,&frame,encodeCopy),
                    Tempest::Metal4InteropResult::Encoded);
          EXPECT_EQ(Tempest::MetalApi::stageMetal4Interop(device,encoder,&frame,encodeCopy),
                    Tempest::Metal4InteropResult::Failed);
          }
        auto fence = device.submit(command);
        if(frame.hold!=nil) {
          command = device.commandBuffer(); // Destroy the wrapper before GPU completion.
          frame.hold.signaledValue = 1;
          }
        if(!fence.wait(5000))
          std::_Exit(2); // Keep in-flight owners alive on a genuine GPU hang.
        for(size_t i=0;i<64;++i)
          EXPECT_EQ(static_cast<const uint8_t*>(output.contents)[i],value);
        EXPECT_GT(Tempest::MetalApi::completedGpuTime(device,fence),0.);
        }
      frame.reject = true;
      {
        auto encoder = command.startEncoding(device);
        EXPECT_EQ(Tempest::MetalApi::stageMetal4Interop(device,encoder,&frame,encodeCopy),
                  Tempest::Metal4InteropResult::Failed);
        }
      EXPECT_TRUE(Tempest::MetalApi::waitIdle(device,0));
      // Discard the unsubmitted sidecar without scheduling its suffix wait.
      command = device.commandBuffer();
      [frame.hold release];
      [residency release]; [descriptor release];
      [source release]; [middle release]; [output release];
      }
    else {
      FAIL() << "Metal 4 validation requires macOS 26";
      }
    }
  }

TEST(MetalApi,Metal4RejectedFence) {
  @autoreleasepool {
    Tempest::Detail::MtDevice device("",true);
    Tempest::Detail::MtMetal4Frame frame(device);
    ASSERT_TRUE(frame.available());
    auto suffix = [(id<MTLCommandQueue>)(void*)device.queue.get() commandBuffer];
    const auto encode = [](void*,MTL::CommandBuffer*,void*,MTL::CommandBuffer*) { return true; };
    ASSERT_TRUE(frame.encode((MTL::CommandBuffer*)(void*)suffix,&device,encode));
    const auto async = device.asyncState();
    const auto token = async->onSubmit();
    ASSERT_TRUE(async->beginCompletion(token));
    async->finishCompletion(token,{Tempest::PresentFailureKind::DeviceLost,-1});
    auto fence = device.aquireFence();
    EXPECT_THROW(frame.submit((MTL::CommandBuffer*)(void*)suffix,fence),Tempest::DeviceLostException);
    EXPECT_EQ(fence->status.load(),MTL::CommandBufferStatusError);
    EXPECT_TRUE(async->waitIdle(0));
    }
  }
