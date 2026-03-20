#include "doctest/doctest.h"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

#include "integration/support/tracking_allocator.h"
#include "maru/maru.h"
#include "maru/vulkan.h"
#include "vulkan_renderer.h"

namespace {
using Clock = std::chrono::steady_clock;

struct SmokeState {
  bool ready = false;
  bool close_requested = false;
  int state_event_count = 0;
  int presentation_event_count = 0;
  int visible_event_count = 0;
  int resizable_event_count = 0;
  MARU_WindowStateChangedFlags last_state_changed_fields = 0;
  MARU_WindowStateChangedFlags last_presentation_changed_fields = 0;
  MARU_WindowPresentationState last_presentation_event_state =
      MARU_WINDOW_PRESENTATION_NORMAL;
  bool last_visible_event_state = true;
  bool last_state_event_visible = true;
  bool last_state_event_focused = false;
  bool last_state_event_resizable = true;
  MARU_WindowPresentationState last_state_event_presentation_state =
      MARU_WINDOW_PRESENTATION_NORMAL;
  std::string trace_log;
};

const char *event_name(MARU_EventId type) {
  switch (type) {
    case MARU_EVENT_CLOSE_REQUESTED:
      return "CLOSE_REQUESTED";
    case MARU_EVENT_WINDOW_RESIZED:
      return "WINDOW_RESIZED";
    case MARU_EVENT_KEY_CHANGED:
      return "KEY_CHANGED";
    case MARU_EVENT_WINDOW_READY:
      return "WINDOW_READY";
    case MARU_EVENT_MOUSE_MOVED:
      return "MOUSE_MOVED";
    case MARU_EVENT_MOUSE_BUTTON_CHANGED:
      return "MOUSE_BUTTON_CHANGED";
    case MARU_EVENT_MOUSE_SCROLLED:
      return "MOUSE_SCROLLED";
    case MARU_EVENT_IDLE_CHANGED:
      return "IDLE_CHANGED";
    case MARU_EVENT_MONITOR_CHANGED:
      return "MONITOR_CHANGED";
    case MARU_EVENT_MONITOR_MODE_CHANGED:
      return "MONITOR_MODE_CHANGED";
    case MARU_EVENT_WINDOW_FRAME:
      return "WINDOW_FRAME";
    case MARU_EVENT_WINDOW_STATE_CHANGED:
      return "WINDOW_STATE_CHANGED";
    case MARU_EVENT_TEXT_EDIT_STARTED:
      return "TEXT_EDIT_STARTED";
    case MARU_EVENT_TEXT_EDIT_UPDATED:
      return "TEXT_EDIT_UPDATED";
    case MARU_EVENT_TEXT_EDIT_COMMITTED:
      return "TEXT_EDIT_COMMITTED";
    case MARU_EVENT_TEXT_EDIT_ENDED:
      return "TEXT_EDIT_ENDED";
    case MARU_EVENT_DROP_ENTERED:
      return "DROP_ENTERED";
    case MARU_EVENT_DROP_HOVERED:
      return "DROP_HOVERED";
    case MARU_EVENT_DROP_EXITED:
      return "DROP_EXITED";
    case MARU_EVENT_DROP_DROPPED:
      return "DROP_DROPPED";
    case MARU_EVENT_DATA_RECEIVED:
      return "DATA_RECEIVED";
    case MARU_EVENT_DATA_REQUESTED:
      return "DATA_REQUESTED";
    case MARU_EVENT_DATA_RELEASED:
      return "DATA_RELEASED";
    case MARU_EVENT_DRAG_FINISHED:
      return "DRAG_FINISHED";
    case MARU_EVENT_CONTROLLER_CHANGED:
      return "CONTROLLER_CHANGED";
    case MARU_EVENT_CONTROLLER_BUTTON_CHANGED:
      return "CONTROLLER_BUTTON_CHANGED";
    case MARU_EVENT_TEXT_EDIT_NAVIGATION:
      return "TEXT_EDIT_NAVIGATION";
    case MARU_EVENT_USER_0:
      return "USER_0";
    case MARU_EVENT_USER_1:
      return "USER_1";
    case MARU_EVENT_USER_2:
      return "USER_2";
    case MARU_EVENT_USER_3:
      return "USER_3";
    case MARU_EVENT_USER_4:
      return "USER_4";
    case MARU_EVENT_USER_5:
      return "USER_5";
    case MARU_EVENT_USER_6:
      return "USER_6";
    case MARU_EVENT_USER_7:
      return "USER_7";
    case MARU_EVENT_USER_8:
      return "USER_8";
    case MARU_EVENT_USER_9:
      return "USER_9";
    case MARU_EVENT_USER_10:
      return "USER_10";
    case MARU_EVENT_USER_11:
      return "USER_11";
    case MARU_EVENT_USER_12:
      return "USER_12";
    case MARU_EVENT_USER_13:
      return "USER_13";
    case MARU_EVENT_USER_14:
      return "USER_14";
    case MARU_EVENT_USER_15:
      return "USER_15";
  }

  return "UNKNOWN";
}

void append_trace_line(std::string *trace_log, const char *line) {
  if (!trace_log || !line) {
    return;
  }
  trace_log->append(line);
  trace_log->push_back('\n');
}

void trace_event(SmokeState *state, MARU_EventId type, const MARU_Event *evt) {
  if (!state) {
    return;
  }

  char line[512];
  int written = std::snprintf(line, sizeof(line),
                              "[presentation-smoke] event=%s (%u)",
                              event_name(type), (unsigned)type);

  if (type == MARU_EVENT_WINDOW_STATE_CHANGED && evt) {
    written += std::snprintf(
        line + (written < 0 ? 0 : written),
        sizeof(line) - (size_t)(written < 0 ? 0 : written),
        " changed_fields=0x%08x presentation_state=%u visible=%u focused=%u"
        " resizable=%u",
        evt->window_state_changed.changed_fields,
        (unsigned)evt->window_state_changed.presentation_state,
        evt->window_state_changed.visible ? 1u : 0u,
        evt->window_state_changed.focused ? 1u : 0u,
        evt->window_state_changed.resizable ? 1u : 0u);
  } else if (type == MARU_EVENT_WINDOW_READY && evt) {
    written += std::snprintf(line + (written < 0 ? 0 : written),
                             sizeof(line) - (size_t)(written < 0 ? 0 : written),
                             " px_size=(%d, %d)",
                             evt->window_ready.geometry.px_size.x,
                             evt->window_ready.geometry.px_size.y);
  } else if (type == MARU_EVENT_WINDOW_RESIZED && evt) {
    written += std::snprintf(
        line + (written < 0 ? 0 : written),
        sizeof(line) - (size_t)(written < 0 ? 0 : written), " px_size=(%d, %d)",
        evt->window_resized.geometry.px_size.x,
        evt->window_resized.geometry.px_size.y);
  }

  append_trace_line(&state->trace_log, line);
}

void smoke_callback(MARU_EventId type, MARU_Window *window,
                    const MARU_Event *evt, void *userdata) {
  (void)window;

  auto *state = static_cast<SmokeState *>(userdata);
  if (!state) {
    return;
  }

  trace_event(state, type, evt);

  if (type == MARU_EVENT_WINDOW_READY) {
    state->ready = true;
    return;
  }

  if (type == MARU_EVENT_CLOSE_REQUESTED) {
    state->close_requested = true;
    return;
  }

  if (type != MARU_EVENT_WINDOW_STATE_CHANGED || !evt) {
    return;
  }

  state->state_event_count++;
  state->last_state_changed_fields = evt->window_state_changed.changed_fields;
  state->last_visible_event_state = evt->window_state_changed.visible;
  state->last_state_event_visible = evt->window_state_changed.visible;
  state->last_state_event_focused = evt->window_state_changed.focused;
  state->last_state_event_resizable = evt->window_state_changed.resizable;
  state->last_state_event_presentation_state =
      evt->window_state_changed.presentation_state;

  if ((evt->window_state_changed.changed_fields &
       MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE) != 0u) {
    state->last_presentation_changed_fields =
        evt->window_state_changed.changed_fields;
    state->last_presentation_event_state =
        evt->window_state_changed.presentation_state;
    state->presentation_event_count++;
  }

  if ((evt->window_state_changed.changed_fields &
       MARU_WINDOW_STATE_CHANGED_VISIBLE) != 0u) {
    state->visible_event_count++;
  }

  if ((evt->window_state_changed.changed_fields &
       MARU_WINDOW_STATE_CHANGED_RESIZABLE) != 0u) {
    state->resizable_event_count++;
  }
}

bool pump_smoke_events(MARU_Context *ctx, SmokeState *state,
                       uint32_t timeout_ms) {
  if (maru_pumpEvents(ctx, timeout_ms, MARU_ALL_EVENTS, smoke_callback,
                      state) != MARU_SUCCESS) {
    return false;
  }

  return !(state && state->close_requested);
}

bool wait_for_window_ready(MARU_Context *ctx, MARU_Window *window,
                           SmokeState *state) {
  const auto deadline = Clock::now() + std::chrono::seconds(2);
  while (Clock::now() < deadline) {
    if (maru_isWindowReady(window) || (state && state->ready)) {
      return true;
    }

    if (!pump_smoke_events(ctx, state, 16)) {
      return false;
    }
  }

  return maru_isWindowReady(window) || (state && state->ready);
}

bool has_vulkan_physical_device(VkInstance instance) {
  uint32_t device_count = 0;
  if (vkEnumeratePhysicalDevices(instance, &device_count, nullptr) !=
      VK_SUCCESS) {
    return false;
  }

  return device_count > 0;
}

struct SmokeWindowSession {
  MARU_IntegrationTrackingAllocator tracking;
  MARU_Context *ctx = nullptr;
  MARU_Window *window = nullptr;
  VulkanRenderer renderer = {};
  bool renderer_initialized = false;
  SmokeState state = {};
  const char *reason = nullptr;
  bool reason_is_skip = false;

  ~SmokeWindowSession() { cleanup(); }

  void cleanup() {
    if (renderer_initialized) {
      if (renderer.ready && renderer.device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(renderer.device);
      }
      vulkan_renderer_cleanup(&renderer);
      renderer_initialized = false;
    }

    if (window) {
      maru_destroyWindow(window);
      window = nullptr;
    }

    if (ctx) {
      maru_destroyContext(ctx);
      ctx = nullptr;
    }
  }

  bool initialize(const char *title) {
    MARU_WindowCreateInfo info = MARU_WINDOW_CREATE_INFO_DEFAULT;
    info.attributes.title = title;
    info.attributes.dip_size = {320, 240};
    info.attributes.visible = true;
    return initialize(info);
  }

  bool initialize(const MARU_WindowCreateInfo &window_info) {
    MARU_ContextCreateInfo ctx_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    tracking.apply(&ctx_info);
    ctx_info.backend = MARU_BACKEND_UNKNOWN;

    if (maru_createContext(&ctx_info, &ctx) != MARU_SUCCESS || !ctx) {
      reason = "desktop context creation failed";
      reason_is_skip = true;
      return false;
    }

    MARU_StringList extensions = {};
    if (maru_getVkExtensions(ctx, &extensions) != MARU_SUCCESS) {
      reason = "maru_getVkExtensions failed";
      return false;
    }

    vulkan_renderer_init(&renderer, extensions.count, extensions.strings);
    renderer_initialized = true;

    if (!has_vulkan_physical_device(renderer.instance)) {
      reason = "no Vulkan-capable physical devices are available";
      reason_is_skip = true;
      return false;
    }

    if (maru_createWindow(ctx, &window_info, &window) != MARU_SUCCESS ||
        !window) {
      reason = "window creation failed";
      reason_is_skip = true;
      return false;
    }

    if (window_info.attributes.visible) {
      return ensure_renderer_ready();
    } else {
      if (!wait_for_window_ready(ctx, window, &state)) {
        reason = "window did not become ready";
        return false;
      }
      return true;
    }
  }

  bool ensure_renderer_ready(uint32_t timeout_ms = 2000) {
    if (renderer.ready) {
      return true;
    }

    if (!wait_for_window_ready(ctx, window, &state)) {
      reason = "window did not become ready";
      return false;
    }

    const auto deadline =
        Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < deadline) {
      if (maru_isWindowReady(window) || state.ready) {
        break;
      }
      if (!pump(timeout_ms > 16 ? 16 : timeout_ms)) {
        return false;
      }
    }

    if (!maru_isWindowReady(window) && !state.ready) {
      reason = "window did not become ready";
      return false;
    }

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (maru_createVkSurface(window, renderer.instance, vkGetInstanceProcAddr,
                             &surface) != MARU_SUCCESS) {
      reason = "maru_createVkSurface failed";
      return false;
    }

    const MARU_WindowGeometry geometry = maru_getWindowGeometry(window);
    vulkan_renderer_setup_surface(&renderer, surface,
                                  (uint32_t)geometry.px_size.x,
                                  (uint32_t)geometry.px_size.y, false);
    return true;
  }

  bool pump(uint32_t timeout_ms = 16) {
    return pump_smoke_events(ctx, &state, timeout_ms);
  }

  bool draw_if_visible() {
    if (!renderer.ready && maru_isWindowVisible(window) &&
        !maru_isWindowMinimized(window)) {
      if (!ensure_renderer_ready()) {
        return false;
      }
    }

    if (renderer.ready && maru_isWindowVisible(window) &&
        !maru_isWindowMinimized(window)) {
      vulkan_renderer_draw_frame(&renderer);
    }
    return !state.close_requested;
  }

  bool pump_and_draw(uint32_t timeout_ms = 16) {
    if (!pump(timeout_ms)) {
      return false;
    }
    return draw_if_visible();
  }

  MARU_WindowPresentationState current_presentation_state() const {
    if (maru_isWindowFullscreen(window)) {
      return MARU_WINDOW_PRESENTATION_FULLSCREEN;
    }
    if (maru_isWindowMinimized(window)) {
      return MARU_WINDOW_PRESENTATION_MINIMIZED;
    }
    return MARU_WINDOW_PRESENTATION_NORMAL;
  }

  bool is_visible() const { return maru_isWindowVisible(window); }
  bool is_resizable() const { return maru_isWindowResizable(window); }
  bool is_focused() const { return maru_isWindowFocused(window); }

  bool wait_for_presentation_state(MARU_WindowPresentationState expected_state,
                                   int previous_event_count,
                                   uint32_t timeout_ms = 2000) {
    const auto deadline =
        Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < deadline) {
      if (state.presentation_event_count > previous_event_count &&
          state.last_presentation_event_state == expected_state &&
          current_presentation_state() == expected_state &&
          (state.last_presentation_changed_fields &
           MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE) != 0u) {
        return true;
      }

      if (!pump_and_draw()) {
        return false;
      }
    }

    return state.presentation_event_count > previous_event_count &&
           state.last_presentation_event_state == expected_state &&
           current_presentation_state() == expected_state &&
           (state.last_presentation_changed_fields &
            MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE) != 0u;
  }

  bool wait_for_visible(bool expected_visible, int previous_visible_event_count,
                        uint32_t timeout_ms = 2000) {
    const auto deadline =
        Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < deadline) {
      if (state.visible_event_count > previous_visible_event_count &&
          state.last_visible_event_state == expected_visible &&
          is_visible() == expected_visible) {
        return true;
      }

      if (!pump_and_draw()) {
        return false;
      }
    }

    return state.visible_event_count > previous_visible_event_count &&
           state.last_visible_event_state == expected_visible &&
           is_visible() == expected_visible;
  }

  bool wait_for_resizable(bool expected_resizable,
                          int previous_resizable_event_count,
                          uint32_t timeout_ms = 2000) {
    const auto deadline =
        Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < deadline) {
      if (state.resizable_event_count > previous_resizable_event_count &&
          state.last_state_event_resizable == expected_resizable &&
          is_resizable() == expected_resizable &&
          (state.last_state_changed_fields &
           MARU_WINDOW_STATE_CHANGED_RESIZABLE) != 0u) {
        return true;
      }

      if (!pump_and_draw()) {
        return false;
      }
    }

    return state.resizable_event_count > previous_resizable_event_count &&
           state.last_state_event_resizable == expected_resizable &&
           is_resizable() == expected_resizable &&
           (state.last_state_changed_fields &
            MARU_WINDOW_STATE_CHANGED_RESIZABLE) != 0u;
  }

  bool wait_for_normal_visible(uint32_t timeout_ms = 2000) {
    const auto deadline =
        Clock::now() + std::chrono::milliseconds(timeout_ms);
    while (Clock::now() < deadline) {
      if (current_presentation_state() == MARU_WINDOW_PRESENTATION_NORMAL &&
          is_visible()) {
        return true;
      }

      if (!pump_and_draw()) {
        return false;
      }
    }

    return current_presentation_state() == MARU_WINDOW_PRESENTATION_NORMAL &&
           is_visible();
  }

  bool check_normal_state() const {
    return current_presentation_state() == MARU_WINDOW_PRESENTATION_NORMAL &&
           !maru_isWindowFullscreen(window) &&
           !maru_isWindowMinimized(window);
  }

  bool check_fullscreen_state() const {
    return current_presentation_state() ==
               MARU_WINDOW_PRESENTATION_FULLSCREEN &&
           maru_isWindowFullscreen(window) &&
           !maru_isWindowMinimized(window);
  }

  bool check_minimized_state() const {
    return current_presentation_state() ==
               MARU_WINDOW_PRESENTATION_MINIMIZED &&
           maru_isWindowMinimized(window) && !maru_isWindowFullscreen(window);
  }

  bool check_hidden_normal_state() const {
    return !is_visible() &&
           current_presentation_state() == MARU_WINDOW_PRESENTATION_NORMAL &&
           !maru_isWindowFullscreen(window) &&
           !maru_isWindowMinimized(window);
  }

  bool last_state_event_matches_accessors() const {
    return state.last_state_event_visible == is_visible() &&
           state.last_state_event_focused == is_focused() &&
           state.last_state_event_resizable == is_resizable() &&
           state.last_state_event_presentation_state ==
               current_presentation_state();
  }

  bool finish() {
    cleanup();
    return is_clean();
  }

  bool is_clean() const { return tracking.is_clean(); }
};

bool initialize_or_skip(SmokeWindowSession *session, const char *title,
                        const char *skip_message) {
  if (!session->initialize(title)) {
    if (session->reason_is_skip) {
      MESSAGE(skip_message, ": ", session->reason);
      return false;
    }
    CHECK_MESSAGE(false, session->reason);
    return false;
  }
  return true;
}

#define TRACE_CHECK(session, expr)                                            \
  do {                                                                        \
    INFO((session).state.trace_log);                                          \
    CHECK(expr);                                                              \
  } while (false)
}  // namespace

TEST_CASE("Presentation.OpenRenderClose") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Smoke Test",
                          "Skipping presentation smoke test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  TRACE_CHECK(session, session.pump_and_draw());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.InitialStateIsNormalVisible") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Initial State Test",
                          "Skipping initial presentation state test")) {
    return;
  }

  TRACE_CHECK(session, session.wait_for_normal_visible());
  TRACE_CHECK(session, session.check_normal_state());
  TRACE_CHECK(session, session.is_visible());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.FullscreenRoundTrip") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Fullscreen Test",
                          "Skipping fullscreen presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  const int enter_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session,
              maru_setWindowFullscreen(session.window, true) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_FULLSCREEN,
                           enter_event_count, 4000));
  TRACE_CHECK(session, session.check_fullscreen_state());
  TRACE_CHECK(session, session.is_visible());

  const int exit_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session,
              maru_setWindowFullscreen(session.window, false) == MARU_SUCCESS);
  TRACE_CHECK(session,
              session.wait_for_presentation_state(MARU_WINDOW_PRESENTATION_NORMAL,
                                                  exit_event_count, 4000));
  TRACE_CHECK(session, session.check_normal_state());
  TRACE_CHECK(session, session.is_visible());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.MinimizeRoundTrip") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Minimize Test",
                          "Skipping minimize presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  const int minimize_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session,
              maru_setWindowMinimized(session.window, true) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_MINIMIZED,
                           minimize_event_count, 4000));
  TRACE_CHECK(session, session.check_minimized_state());
  TRACE_CHECK(session, !session.is_visible());

  const int restore_visible_event_count = session.state.visible_event_count;
  const int restore_presentation_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session,
              maru_setWindowMinimized(session.window, false) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_NORMAL,
                           restore_presentation_event_count, 4000));
  TRACE_CHECK(session,
              session.wait_for_visible(true, restore_visible_event_count, 4000));
  TRACE_CHECK(session, session.check_normal_state());
  TRACE_CHECK(session, session.is_visible());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.HiddenVisibleRoundTrip") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Visibility Test",
                          "Skipping visibility presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  const int hide_event_count = session.state.visible_event_count;
  TRACE_CHECK(session,
              maru_setWindowVisible(session.window, false) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_visible(false, hide_event_count));
  TRACE_CHECK(session, session.check_hidden_normal_state());

  const int show_event_count = session.state.visible_event_count;
  TRACE_CHECK(session,
              maru_setWindowVisible(session.window, true) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_visible(true, show_event_count));
  TRACE_CHECK(session, session.check_normal_state());
  TRACE_CHECK(session, session.is_visible());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.DirectUpdateFullscreenRoundTrip") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Direct Update Fullscreen Test",
                          "Skipping direct-update fullscreen presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());

  MARU_WindowAttributes attrs = {};
  attrs.presentation_state = MARU_WINDOW_PRESENTATION_FULLSCREEN;
  attrs.visible = true;
  const int enter_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session, maru_updateWindow(session.window,
                                         MARU_WINDOW_ATTR_PRESENTATION_STATE |
                                             MARU_WINDOW_ATTR_VISIBLE,
                                         &attrs) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_FULLSCREEN,
                           enter_event_count, 4000));
  TRACE_CHECK(session, session.check_fullscreen_state());

  attrs.presentation_state = MARU_WINDOW_PRESENTATION_NORMAL;
  attrs.visible = true;
  const int exit_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session, maru_updateWindow(session.window,
                                         MARU_WINDOW_ATTR_PRESENTATION_STATE |
                                             MARU_WINDOW_ATTR_VISIBLE,
                                         &attrs) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_NORMAL,
                           exit_event_count, 4000));
  TRACE_CHECK(session, session.check_normal_state());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.DirectUpdateVisibilityRoundTrip") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Direct Update Visibility Test",
                          "Skipping direct-update visibility presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());

  MARU_WindowAttributes attrs = {};
  attrs.visible = false;
  const int hide_event_count = session.state.visible_event_count;
  TRACE_CHECK(session, maru_updateWindow(session.window, MARU_WINDOW_ATTR_VISIBLE,
                                         &attrs) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_visible(false, hide_event_count));
  TRACE_CHECK(session, session.check_hidden_normal_state());

  attrs.visible = true;
  const int show_event_count = session.state.visible_event_count;
  TRACE_CHECK(session, maru_updateWindow(session.window, MARU_WINDOW_ATTR_VISIBLE,
                                         &attrs) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_visible(true, show_event_count));
  TRACE_CHECK(session, session.check_normal_state());
  TRACE_CHECK(session, session.is_visible());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.CreateFullscreen") {
  SmokeWindowSession session;
  MARU_WindowCreateInfo info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  info.attributes.title = "Presentation Create Fullscreen Test";
  info.attributes.dip_size = {320, 240};
  info.attributes.visible = true;
  info.attributes.presentation_state = MARU_WINDOW_PRESENTATION_FULLSCREEN;

  if (!session.initialize(info)) {
    if (session.reason_is_skip) {
      MESSAGE("Skipping create-fullscreen presentation test: ", session.reason);
      return;
    }
    CHECK_MESSAGE(false, session.reason);
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  TRACE_CHECK(session, session.check_fullscreen_state());
  TRACE_CHECK(session, session.is_visible());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.CreateMinimized") {
  SmokeWindowSession session;
  MARU_WindowCreateInfo info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  info.attributes.title = "Presentation Create Minimized Test";
  info.attributes.dip_size = {320, 240};
  info.attributes.visible = false;
  info.attributes.presentation_state = MARU_WINDOW_PRESENTATION_MINIMIZED;

  if (!session.initialize(info)) {
    if (session.reason_is_skip) {
      MESSAGE("Skipping create-minimized presentation test: ", session.reason);
      return;
    }
    CHECK_MESSAGE(false, session.reason);
    return;
  }

  TRACE_CHECK(session, session.check_minimized_state());
  TRACE_CHECK(session, !session.is_visible());

  const int restore_visible_event_count = session.state.visible_event_count;
  const int restore_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session,
              maru_setWindowMinimized(session.window, false) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_NORMAL,
                           restore_event_count, 4000));
  TRACE_CHECK(session,
              session.wait_for_visible(true, restore_visible_event_count, 4000));
  TRACE_CHECK(session, session.check_normal_state());
  TRACE_CHECK(session, session.is_visible());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.CreateHiddenNormal") {
  SmokeWindowSession session;
  MARU_WindowCreateInfo info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  info.attributes.title = "Presentation Create Hidden Normal Test";
  info.attributes.dip_size = {320, 240};
  info.attributes.visible = false;
  info.attributes.presentation_state = MARU_WINDOW_PRESENTATION_NORMAL;

  if (!session.initialize(info)) {
    if (session.reason_is_skip) {
      MESSAGE("Skipping create-hidden-normal presentation test: ",
              session.reason);
      return;
    }
    CHECK_MESSAGE(false, session.reason);
    return;
  }

  TRACE_CHECK(session, session.check_hidden_normal_state());

  const int show_event_count = session.state.visible_event_count;
  TRACE_CHECK(session,
              maru_setWindowVisible(session.window, true) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_visible(true, show_event_count, 4000));
  TRACE_CHECK(session, session.check_normal_state());
  TRACE_CHECK(session, session.is_visible());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.ResizableRoundTrip") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Resizable Test",
                          "Skipping resizable presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  const int disable_event_count = session.state.resizable_event_count;
  TRACE_CHECK(session,
              maru_setWindowResizable(session.window, false) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_resizable(false, disable_event_count));
  TRACE_CHECK(session, !session.is_resizable());
  TRACE_CHECK(session, session.last_state_event_matches_accessors());

  const int enable_event_count = session.state.resizable_event_count;
  TRACE_CHECK(session,
              maru_setWindowResizable(session.window, true) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_resizable(true, enable_event_count));
  TRACE_CHECK(session, session.is_resizable());
  TRACE_CHECK(session, session.last_state_event_matches_accessors());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.StatePayloadMatchesAccessorsOnFullscreen") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Payload Fullscreen Test",
                          "Skipping payload/accessor fullscreen presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  const int enter_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session,
              maru_setWindowFullscreen(session.window, true) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_FULLSCREEN,
                           enter_event_count, 4000));
  TRACE_CHECK(session, session.last_state_event_matches_accessors());

  const int exit_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session,
              maru_setWindowFullscreen(session.window, false) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_NORMAL,
                           exit_event_count, 4000));
  TRACE_CHECK(session, session.last_state_event_matches_accessors());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.StatePayloadMatchesAccessorsOnVisibility") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Payload Visibility Test",
                          "Skipping payload/accessor visibility presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  const int hide_event_count = session.state.visible_event_count;
  TRACE_CHECK(session,
              maru_setWindowVisible(session.window, false) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_visible(false, hide_event_count));
  TRACE_CHECK(session, session.last_state_event_matches_accessors());

  const int show_event_count = session.state.visible_event_count;
  TRACE_CHECK(session,
              maru_setWindowVisible(session.window, true) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_visible(true, show_event_count));
  TRACE_CHECK(session, session.last_state_event_matches_accessors());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.StatePayloadMatchesAccessorsOnResizable") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Payload Resizable Test",
                          "Skipping payload/accessor resizable presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  const int disable_event_count = session.state.resizable_event_count;
  TRACE_CHECK(session,
              maru_setWindowResizable(session.window, false) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_resizable(false, disable_event_count));
  TRACE_CHECK(session, session.last_state_event_matches_accessors());

  const int enable_event_count = session.state.resizable_event_count;
  TRACE_CHECK(session,
              maru_setWindowResizable(session.window, true) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_resizable(true, enable_event_count));
  TRACE_CHECK(session, session.last_state_event_matches_accessors());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.DirectUpdateMinimizeRoundTrip") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Direct Update Minimize Test",
                          "Skipping direct-update minimize presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());

  MARU_WindowAttributes attrs = {};
  attrs.presentation_state = MARU_WINDOW_PRESENTATION_MINIMIZED;
  attrs.visible = false;
  const int minimize_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session, maru_updateWindow(session.window,
                                         MARU_WINDOW_ATTR_PRESENTATION_STATE |
                                             MARU_WINDOW_ATTR_VISIBLE,
                                         &attrs) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_MINIMIZED,
                           minimize_event_count, 4000));
  TRACE_CHECK(session, session.check_minimized_state());
  TRACE_CHECK(session, !session.is_visible());

  attrs.presentation_state = MARU_WINDOW_PRESENTATION_NORMAL;
  attrs.visible = true;
  const int restore_visible_event_count = session.state.visible_event_count;
  const int restore_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session, maru_updateWindow(session.window,
                                         MARU_WINDOW_ATTR_PRESENTATION_STATE |
                                             MARU_WINDOW_ATTR_VISIBLE,
                                         &attrs) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_NORMAL,
                           restore_event_count, 4000));
  TRACE_CHECK(session,
              session.wait_for_visible(true, restore_visible_event_count, 4000));
  TRACE_CHECK(session, session.check_normal_state());
  TRACE_CHECK(session, session.is_visible());
  TRACE_CHECK(session, session.finish());
}

TEST_CASE("Presentation.StatePayloadMatchesAccessorsOnMinimize") {
  SmokeWindowSession session;
  if (!initialize_or_skip(&session, "Presentation Payload Minimize Test",
                          "Skipping payload/accessor minimize presentation test")) {
    return;
  }

  TRACE_CHECK(session, session.pump_and_draw());
  const int minimize_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session,
              maru_setWindowMinimized(session.window, true) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_MINIMIZED,
                           minimize_event_count, 4000));
  TRACE_CHECK(session, session.last_state_event_matches_accessors());

  const int restore_visible_event_count = session.state.visible_event_count;
  const int restore_event_count = session.state.presentation_event_count;
  TRACE_CHECK(session,
              maru_setWindowMinimized(session.window, false) == MARU_SUCCESS);
  TRACE_CHECK(session, session.wait_for_presentation_state(
                           MARU_WINDOW_PRESENTATION_NORMAL,
                           restore_event_count, 4000));
  TRACE_CHECK(session,
              session.wait_for_visible(true, restore_visible_event_count, 4000));
  TRACE_CHECK(session, session.last_state_event_matches_accessors());
  TRACE_CHECK(session, session.finish());
}
