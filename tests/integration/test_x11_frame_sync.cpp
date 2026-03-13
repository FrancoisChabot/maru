#include "doctest/doctest.h"
#include <atomic>
#include <chrono>
#include <iostream>

#include "integration/support/tracking_allocator.h"
#include "maru/native/x11.h"
#include "maru/maru.h"

namespace {
using Clock = std::chrono::steady_clock;

struct FrameSyncState {
  std::atomic<int> frame_count{0};
  Clock::time_point start_time;
};

void frame_sync_callback(MARU_EventId type, MARU_Window *window,
                         const MARU_Event *evt, void *userdata) {
  (void)evt;
  auto *state = static_cast<FrameSyncState *>(userdata);
  if (type == MARU_EVENT_WINDOW_FRAME) {
    state->frame_count++;
    // Request next frame immediately to stress the throttling
    maru_requestWindowFrame(window);
  }
}
} // namespace

TEST_CASE("X11.FrameSynchronizationThrottling") {
  MARU_ContextCreateInfo ctx_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  ctx_info.backend = MARU_BACKEND_X11; 

  MARU_Context *ctx = nullptr;
  if (maru_createContext(&ctx_info, &ctx) != MARU_SUCCESS) {
    MESSAGE("Skipping X11 frame sync test: X11 context creation failed (likely no X11 display).");
    return;
  }

  if (!maru_getX11SupportsExtendedFrameSync(ctx)) {
    maru_destroyContext(ctx);
    MESSAGE("Skipping X11 frame sync test: compositor does not advertise extended frame sync support.");
    return;
  }

  MARU_WindowCreateInfo win_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  win_info.attributes.title = "Frame Sync Test";
  win_info.attributes.dip_size = {100, 100};
  win_info.attributes.visible = true;

  MARU_Window *window = nullptr;
  if (maru_createWindow(ctx, &win_info, &window) != MARU_SUCCESS) {
    maru_destroyContext(ctx);
    MESSAGE("Skipping X11 frame sync test: window creation failed.");
    return;
  }

  FrameSyncState state;
  state.start_time = Clock::now();

  // Initial frame request
  maru_requestWindowFrame(window);

  // Run for 1 second
  while (Clock::now() - state.start_time < std::chrono::seconds(1)) {
    maru_pumpEvents(ctx, 16, MARU_ALL_EVENTS, frame_sync_callback, &state);
  }

  int total_frames = state.frame_count.load();
  MESSAGE("Total frames in 1 second: ", total_frames);

  CHECK(total_frames > 0);
  CHECK(total_frames < 300);

  maru_destroyWindow(window);
  maru_destroyContext(ctx);
}
