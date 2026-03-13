#include "doctest/doctest.h"

#include "maru/maru.h"
#include "integration/support/tracking_allocator.h"

TEST_CASE("DesktopIntegration.ProbeDesktopEnvironment") {
  MARU_IntegrationTrackingAllocator tracking;
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  tracking.apply(&create_info);
  create_info.backend = MARU_BACKEND_UNKNOWN;

  MARU_Context *ctx = nullptr;
  MARU_Status status = maru_createContext(&create_info, &ctx);
  if (status == MARU_SUCCESS && ctx) {
    maru_destroyContext(ctx);
    CHECK(tracking.is_clean());
  } else {
    MESSAGE("Context creation unavailable; skipping create/destroy assertion.");
  }
}

TEST_CASE("DesktopIntegration.ContextLifecycleCreateDestroyRepeated") {
  for (uint32_t i = 0; i < 5; ++i) {
    MARU_IntegrationTrackingAllocator tracking;
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    tracking.apply(&create_info);
    create_info.backend = MARU_BACKEND_UNKNOWN;

    MARU_Context *ctx = nullptr;
    MARU_Status status = maru_createContext(&create_info, &ctx);
    if (status == MARU_SUCCESS && ctx) {
      maru_destroyContext(ctx);
      CHECK(tracking.is_clean());
    } else {
      MESSAGE("Context creation unavailable for iteration " << i << "; skipping create/destroy assertion.");
    }
  }
}
