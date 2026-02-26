#include "utest.h"
#include "maru/maru.h"

UTEST(LinuxWorker, CreateDestroyContext) {
  MARU_ContextCreateInfo create_info = {0};
  create_info.backend = MARU_BACKEND_UNKNOWN; // Will pick Wayland or X11 on Linux
  
  MARU_Context *ctx = NULL;
  MARU_Status status = maru_createContext(&create_info, &ctx);
  
  // On some CI environments, it might fail if no display is available,
  // but if it succeeds, destruction should also succeed and not hang.
  if (status == MARU_SUCCESS) {
    status = maru_destroyContext(ctx);
    EXPECT_EQ(status, MARU_SUCCESS);
  }
}

UTEST(LinuxWorker, MultipleCreateDestroy) {
  for (int i = 0; i < 5; ++i) {
    MARU_ContextCreateInfo create_info = {0};
    MARU_Context *ctx = NULL;
    MARU_Status status = maru_createContext(&create_info, &ctx);
    if (status == MARU_SUCCESS) {
      EXPECT_EQ(maru_destroyContext(ctx), MARU_SUCCESS);
    }
  }
}
