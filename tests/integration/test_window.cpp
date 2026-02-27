#include <gtest/gtest.h>

#include "integration/harness/test_session.h"

namespace {

struct WindowLifecycleState {
  bool saw_ready = false;
};

void WindowLifecycleEventCb(MARU_EventId type, MARU_Window *window,
                            const MARU_Event *evt, void *userdata) {
  (void)window;
  (void)evt;
  WindowLifecycleState *state = (WindowLifecycleState *)userdata;
  if (!state) {
    return;
  }
  if (type == MARU_EVENT_WINDOW_READY) {
    state->saw_ready = true;
  }
}

}  // namespace

TEST(DesktopIntegration, WindowLifecycleReadyCloseDestroy) {
  MARU_TestSession session = {};
  char reason[256] = {};
  ASSERT_TRUE(maru_test_session_create(&session, MARU_BACKEND_UNKNOWN, reason,
                                       sizeof(reason)))
      << reason;
  ASSERT_NE(session.context, nullptr);
  ASSERT_NE(session.window, nullptr);

  WindowLifecycleState state = {};
  for (uint32_t i = 0; i < 120 && !state.saw_ready; ++i) {
    ASSERT_EQ(maru_pumpEvents(session.context, 16, WindowLifecycleEventCb, &state),
              MARU_SUCCESS);
  }
  ASSERT_TRUE(state.saw_ready);

  ASSERT_EQ(maru_destroyWindow(session.window), MARU_SUCCESS);
  session.window = nullptr;

  maru_test_session_destroy(&session);
  EXPECT_TRUE(session.tracking_allocator.is_clean());
}
