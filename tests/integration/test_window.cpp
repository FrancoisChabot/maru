#include "doctest/doctest.h"

#include <atomic>
#include <chrono>
#include <thread>

#include "maru/maru.h"
#include "integration/support/tracking_allocator.h"

namespace {
using Clock = std::chrono::steady_clock;

struct TestEventCallback {
  std::atomic<bool> called{false};
  MARU_EventId last_type = (MARU_EventId)-1;
};

void test_callback(MARU_EventId type, MARU_Window *window, const MARU_Event *evt,
                   void *userdata) {
  (void)window;
  (void)evt;
  auto *cb = static_cast<TestEventCallback *>(userdata);
  cb->called = true;
  cb->last_type = type;
}
}  // namespace

TEST_CASE("DesktopIntegration.CrossThreadPostEventWithoutWindow") {
  MARU_IntegrationTrackingAllocator tracking;
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  tracking.apply(&create_info);
  create_info.backend = MARU_BACKEND_UNKNOWN;

  MARU_Context *ctx = nullptr;
  MARU_Status status = maru_createContext(&create_info, &ctx);
  if (status != MARU_SUCCESS || !ctx) {
    MESSAGE("Context creation unavailable; skipping cross-thread test.");
    return;
  }

  TestEventCallback cb;
  const MARU_EventId test_type = MARU_EVENT_USER_0;
  MARU_UserDefinedEvent user_evt = {0};

  std::thread poster([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    maru_postEvent(ctx, test_type, nullptr, user_evt);
  });

  auto started = Clock::now();
  status = MARU_FAILURE;
  while (Clock::now() - started < std::chrono::seconds(2)) {
    status = maru_pumpEvents(ctx, 100, test_callback, &cb);
    if (cb.called) break;
  }

  poster.join();

  REQUIRE(status == MARU_SUCCESS);
  CHECK(cb.called);
  CHECK(cb.last_type == test_type);

  CHECK(maru_destroyContext(ctx) == MARU_SUCCESS);
  CHECK(tracking.is_clean());
}

TEST_CASE("DesktopIntegration.CrossThreadWakeContextWithoutWindow") {
  MARU_IntegrationTrackingAllocator tracking;
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  tracking.apply(&create_info);
  create_info.backend = MARU_BACKEND_UNKNOWN;

  MARU_Context *ctx = nullptr;
  MARU_Status status = maru_createContext(&create_info, &ctx);
  if (status != MARU_SUCCESS || !ctx) {
    MESSAGE("Context creation unavailable; skipping cross-thread test.");
    return;
  }

  TestEventCallback cb;
  std::thread waker([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    maru_wakeContext(ctx);
  });

  auto started = Clock::now();
  // We expect maru_pumpEvents to return SUCCESS even if no events were processed
  // but it was woken up. 
  status = maru_pumpEvents(ctx, 2000, test_callback, &cb);
  
  auto elapsed_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - started)
          .count();
  waker.join();

  REQUIRE(status == MARU_SUCCESS);
  CHECK(elapsed_ms < 1000);

  CHECK(maru_destroyContext(ctx) == MARU_SUCCESS);
  CHECK(tracking.is_clean());
}
