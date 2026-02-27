#include <gtest/gtest.h>

#include "maru/maru.h"
#include "integration/harness/tracking_allocator.h"

TEST(DesktopIntegration, ContextCreateOOMReplayPerAllocationEvent) {
  MARU_ContextCreateInfo base_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  base_info.backend = MARU_BACKEND_UNKNOWN;

  MARU_IntegrationTrackingAllocator baseline_tracking;

  MARU_ContextCreateInfo create_info = base_info;
  baseline_tracking.apply(&create_info);

  MARU_Context *context = nullptr;
  ASSERT_EQ(maru_createContext(&create_info, &context), MARU_SUCCESS);
  ASSERT_NE(context, nullptr);
  EXPECT_EQ(maru_destroyContext(context), MARU_SUCCESS);
  EXPECT_TRUE(baseline_tracking.is_clean());

  const size_t alloc_events = baseline_tracking.alloc_event_count();
  EXPECT_GT(alloc_events, 0u);
  for (size_t fail_event = 1u; fail_event <= alloc_events; ++fail_event) {
    MARU_IntegrationTrackingAllocator tracking;
    tracking.set_oom(true, fail_event);

    MARU_ContextCreateInfo trial_info = base_info;
    tracking.apply(&trial_info);

    MARU_Context *trial_ctx = nullptr;
    MARU_Status trial_status = maru_createContext(&trial_info, &trial_ctx);
    if (trial_status == MARU_SUCCESS && trial_ctx) {
      EXPECT_EQ(maru_destroyContext(trial_ctx), MARU_SUCCESS);
    }

    EXPECT_TRUE(tracking.oom_triggered())
        << "Configured OOM event was not reached.";
    EXPECT_TRUE(tracking.is_clean());
  }
}
