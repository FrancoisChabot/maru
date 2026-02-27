#include <gtest/gtest.h>

#include <cstring>

#include "integration/harness/test_session.h"

namespace {

struct IntegrationPumpStats {
  uint32_t total_events = 0;
  uint32_t ready_events = 0;
};

void IntegrationEventCb(MARU_EventId type, MARU_Window *window,
                        const MARU_Event *evt, void *userdata) {
  (void)window;
  (void)evt;
  IntegrationPumpStats *stats = (IntegrationPumpStats *)userdata;
  if (!stats) {
    return;
  }
  stats->total_events++;
  if (type == MARU_EVENT_WINDOW_READY) {
    stats->ready_events++;
  }
}

}  // namespace

TEST(DesktopIntegration, ProbeDesktopEnvironment) {
  MARU_Context *ctx = nullptr;
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  create_info.backend = MARU_BACKEND_UNKNOWN;
  ASSERT_EQ(maru_createContext(&create_info, &ctx), MARU_SUCCESS);
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ(maru_destroyContext(ctx), MARU_SUCCESS);
}

TEST(DesktopIntegration, CreateContextWindowAndPump) {
  MARU_TestSession session = {};
  char reason[256] = {};

  ASSERT_TRUE(maru_test_session_create(&session, MARU_BACKEND_UNKNOWN, reason,
                                       sizeof(reason)))
      << reason;

  IntegrationPumpStats stats = {};
  for (uint32_t i = 0; i < 8; ++i) {
    MARU_Status status =
        maru_pumpEvents(session.context, 16, IntegrationEventCb, &stats);
    EXPECT_EQ(status, MARU_SUCCESS);
    if (status != MARU_SUCCESS) {
      break;
    }
  }

  EXPECT_GE(stats.total_events, stats.ready_events);
  maru_test_session_destroy(&session);
  EXPECT_TRUE(session.tracking_allocator.is_clean());
}

TEST(DesktopIntegration, AutomationDriverBinding) {
  MARU_TestSession session = {};
  char reason[256] = {};
  ASSERT_TRUE(maru_test_session_create(&session, MARU_BACKEND_UNKNOWN, reason,
                                       sizeof(reason)))
      << reason;

  ASSERT_NE(session.driver, nullptr);
  ASSERT_NE(session.driver->name(), nullptr);
  EXPECT_GT(std::strlen(session.driver->name()), 0u);

  maru_test_session_destroy(&session);
  EXPECT_TRUE(session.tracking_allocator.is_clean());
}

TEST(DesktopIntegration, ContextLifecycleCreateDestroySingle) {
  MARU_Context *ctx = nullptr;
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  MARU_IntegrationTrackingAllocator tracking;
  tracking.apply(&create_info);
  create_info.backend = MARU_BACKEND_UNKNOWN;
  ASSERT_EQ(maru_createContext(&create_info, &ctx), MARU_SUCCESS);
  ASSERT_NE(ctx, nullptr);
  EXPECT_EQ(maru_destroyContext(ctx), MARU_SUCCESS);
  EXPECT_TRUE(tracking.is_clean());
}

TEST(DesktopIntegration, ContextLifecycleCreateDestroyRepeated) {
  for (uint32_t i = 0; i < 5; ++i) {
    MARU_Context *ctx = nullptr;
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_IntegrationTrackingAllocator tracking;
    tracking.apply(&create_info);
    create_info.backend = MARU_BACKEND_UNKNOWN;
    ASSERT_EQ(maru_createContext(&create_info, &ctx), MARU_SUCCESS);
    ASSERT_NE(ctx, nullptr);
    EXPECT_EQ(maru_destroyContext(ctx), MARU_SUCCESS);
    EXPECT_TRUE(tracking.is_clean());
  }
}
