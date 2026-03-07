#include "doctest/doctest.h"

#include "maru/maru.h"
#include "integration/support/tracking_allocator.h"

TEST_CASE("DesktopIntegration.ContextCreateOOMReplayPerAllocationEvent") {
  MARU_ContextCreateInfo base_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  base_info.backend = MARU_BACKEND_UNKNOWN;

  MARU_IntegrationTrackingAllocator baseline_tracking;
  MARU_ContextCreateInfo create_info = base_info;
  baseline_tracking.apply(&create_info);

  MARU_Context *context = nullptr;
  const MARU_Status baseline_status = maru_createContext(&create_info, &context);
  if (baseline_status != MARU_SUCCESS || !context) {
    MESSAGE("Skipping test because context creation is unavailable in this environment.");
    return;
  }

  CHECK(maru_destroyContext(context) == MARU_SUCCESS);
  CHECK(baseline_tracking.is_clean());

  const size_t alloc_events = baseline_tracking.alloc_event_count();

  CHECK(alloc_events > 0u);
  for (size_t fail_event = 1u; fail_event <= alloc_events; ++fail_event) {
    MARU_IntegrationTrackingAllocator tracking;
    tracking.set_oom(true, fail_event);

    MARU_ContextCreateInfo trial_info = base_info;
    tracking.apply(&trial_info);

    MARU_Context *trial_ctx = nullptr;
    MARU_Status trial_status = maru_createContext(&trial_info, &trial_ctx);

    if (trial_status == MARU_SUCCESS && trial_ctx) {
      CHECK(maru_destroyContext(trial_ctx) == MARU_SUCCESS);
    }

    CHECK_MESSAGE(tracking.oom_triggered(),
                  "Configured OOM event was not reached for iteration " << fail_event);
    CHECK(tracking.is_clean());
  }
}
