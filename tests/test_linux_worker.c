#include "utest.h"
#include "maru/maru.h"
#include "maru_test_utils.h"

UTEST(LinuxWorker, CreateDestroyContext) {
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  MARU_TestTrackingAllocator tracking = {0};
  maru_test_tracking_allocator_init(&tracking);
  maru_test_tracking_allocator_apply(&tracking, &create_info);
  create_info.backend = MARU_BACKEND_UNKNOWN; // Will pick Wayland or X11 on Linux
  
  MARU_Context *ctx = NULL;
  MARU_Status status = maru_createContext(&create_info, &ctx);
  
  // On some CI environments, it might fail if no display is available,
  // but if it succeeds, destruction should also succeed and not hang.
  if (status == MARU_SUCCESS) {
    status = maru_destroyContext(ctx);
    EXPECT_EQ(status, MARU_SUCCESS);
    EXPECT_TRUE(maru_test_tracking_allocator_is_clean(&tracking));
  }
  maru_test_tracking_allocator_shutdown(&tracking);
}

UTEST(LinuxWorker, MultipleCreateDestroy) {
  for (int i = 0; i < 5; ++i) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_TestTrackingAllocator tracking = {0};
    maru_test_tracking_allocator_init(&tracking);
    maru_test_tracking_allocator_apply(&tracking, &create_info);
    MARU_Context *ctx = NULL;
    MARU_Status status = maru_createContext(&create_info, &ctx);
    if (status == MARU_SUCCESS) {
      EXPECT_EQ(maru_destroyContext(ctx), MARU_SUCCESS);
      EXPECT_TRUE(maru_test_tracking_allocator_is_clean(&tracking));
    }
    maru_test_tracking_allocator_shutdown(&tracking);
  }
}

UTEST(LinuxWorker, GetControllers) {
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  MARU_TestTrackingAllocator tracking = {0};
  maru_test_tracking_allocator_init(&tracking);
  maru_test_tracking_allocator_apply(&tracking, &create_info);
  MARU_Context *ctx = NULL;
  if (maru_createContext(&create_info, &ctx) == MARU_SUCCESS) {
    MARU_ControllerList list;
    EXPECT_EQ(maru_getControllers(ctx, &list), MARU_SUCCESS);
    // On a CI machine, list.count is likely 0, but the call should succeed.
    maru_destroyContext(ctx);
    EXPECT_TRUE(maru_test_tracking_allocator_is_clean(&tracking));
  }
  maru_test_tracking_allocator_shutdown(&tracking);
}
