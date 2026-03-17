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

  maru_destroyContext(context);
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
      maru_destroyContext(trial_ctx);
    }

    CHECK_MESSAGE(tracking.oom_triggered(),
                  "Configured OOM event was not reached for iteration " << fail_event);
    CHECK(tracking.is_clean());
  }
}

TEST_CASE("DesktopIntegration.ContextUpdateAttributes") {
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  create_info.backend = MARU_BACKEND_UNKNOWN;

  MARU_Context *ctx = nullptr;
  if (maru_createContext(&create_info, &ctx) != MARU_SUCCESS) {
    return;
  }

  SUBCASE("Inhibit Idle Toggle") {
    CHECK(maru_setContextInhibitsSystemIdle(ctx, true) == MARU_SUCCESS);
    // There is no easy way to verify OS side effect here, but we check for crashes/errors.
    CHECK(maru_setContextInhibitsSystemIdle(ctx, false) == MARU_SUCCESS);
  }

  SUBCASE("Idle Timeout") {
    CHECK(maru_setContextIdleTimeout(ctx, 1000) == MARU_SUCCESS);
    CHECK(maru_setContextIdleTimeout(ctx, 0) == MARU_SUCCESS);
  }

  maru_destroyContext(ctx);
}
