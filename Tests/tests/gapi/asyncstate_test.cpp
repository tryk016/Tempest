#include <gtest/gtest.h>

#include "../../../Engine/gapi/metal/mtasyncstate.h"

#include <atomic>
#include <thread>

using namespace Tempest;
using namespace Tempest::Detail;

TEST(main,MetalAsyncPresentMailboxFirstFailureWins) {
  MtAsyncState state;
  EXPECT_FALSE(state.takePresentFailure());

  PresentFailure first;
  first.kind       = PresentFailureKind::Timeout;
  first.statusCode = 5;
  first.nativeCode = 2;
  first.serial     = 10;

  PresentFailure second;
  second.kind       = PresentFailureKind::DeviceLost;
  second.statusCode = 5;
  second.nativeCode = 11;
  second.serial     = 11;

  const auto firstToken  = state.onSubmit();
  const auto secondToken = state.onSubmit();
  ASSERT_TRUE(state.beginCompletion(firstToken));
  state.finishCompletion(firstToken,first);
  ASSERT_TRUE(state.beginCompletion(secondToken));
  state.finishCompletion(secondToken,second);

  const auto result = state.takePresentFailure();
  EXPECT_EQ(result.kind,PresentFailureKind::Timeout);
  EXPECT_EQ(result.statusCode,5);
  EXPECT_EQ(result.nativeCode,2);
  EXPECT_EQ(result.serial,10u);
  EXPECT_FALSE(state.takePresentFailure());
  }

TEST(main,MetalAsyncPresentFaultIsOneShot) {
  MtAsyncState state;
  PresentFailure completed;
  completed.statusCode = 4;
  completed.serial     = 1;

  auto token = state.onSubmit();
  ASSERT_TRUE(state.beginCompletion(token));
  state.finishCompletion(token,completed,true);
  EXPECT_EQ(state.takePresentFailure().kind,
            PresentFailureKind::DeviceLost);

  completed.serial = 2;
  token = state.onSubmit();
  ASSERT_TRUE(state.beginCompletion(token));
  state.finishCompletion(token,completed,true);
  EXPECT_FALSE(state.takePresentFailure());
  }

TEST(main,MetalAsyncPresentPublishesBeforeIdle) {
  MtAsyncState state;
  std::atomic_bool waiterSawFailure = false;

  const auto token = state.onSubmit();
  std::thread waiter([&](){
    state.waitIdle();
    waiterSawFailure.store(bool(state.takePresentFailure()),
                           std::memory_order_release);
    });

  PresentFailure failure;
  failure.kind       = PresentFailureKind::Internal;
  failure.statusCode = 5;
  failure.nativeCode = 1;
  failure.serial     = 42;
  ASSERT_TRUE(state.beginCompletion(token));
  state.finishCompletion(token,failure);
  waiter.join();

  EXPECT_TRUE(waiterSawFailure.load(std::memory_order_acquire));
  }

TEST(main,MetalAsyncCompletionTokenIsExactlyOnce) {
  MtAsyncState state;

  const auto first = state.onSubmit();
  ASSERT_TRUE(state.beginCompletion(first));
  EXPECT_FALSE(state.beginCompletion(first));
  state.finishCompletion(first);

  const auto reused = state.onSubmit();
  EXPECT_FALSE(state.beginCompletion(first));
  ASSERT_TRUE(state.beginCompletion(reused));
  state.finishCompletion(reused);
  state.waitIdle();
  }
