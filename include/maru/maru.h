// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_H_INCLUDED
#define MARU_H_INCLUDED

/*
 * Threading and memory model
 *
 * MARU follows a "single-owner, multiple-reader" model:
 *
 * 1. The thread that creates a MARU_Context is its owner thread. For the
 *    broadest cross-platform parity, make this your main thread.
 * 2. Unless documented otherwise, functions returning MARU_Status are owner-
 *    thread APIs and must be called from that thread.
 * 3. Direct-return accessors can be called from any thread with external
 *    synchronization to guarantee visibility with the owner thread.
 * 4. maru_postEvent(), maru_wakeContext(), maru_retain*(), maru_release*(),
 *    and maru_getVersion() are globally thread-safe.
 * 5. Backends may use internal helper threads for blocking OS work. These
 *    helpers never invoke your MARU_EventCallback directly; event callbacks are
 *    still only dispatched inline from maru_pumpEvents() on the owner thread.
 *
 * Handle layout note
 *
 * Public handles are API-opaque. Internal details headers expose a stable
 * prefix used to implement low-overhead inline accessors. Do not downcast the
 * handles yourself; use the public accessors below.
 *
 * Ownership and lifetime model
 *
 * 1. Handle arguments are borrowed references. MARU does not take ownership of
 *    passed-in handles unless an API explicitly says so.
 * 2. Text and pixel payloads that MARU persists after a successful call are
 *    copied before that call returns.
 * 3. Lists, strings, and buffers returned by getters or exposed through event
 *    payloads are borrowed views. Copy what you need if it must outlive the
 *    documented lifetime.
 *
 * Validation and build configuration note
 *
 * This header includes a generated maru_config.h exposing the build-time
 * feature macros used for this build, such as MARU_VALIDATE_API_CALLS,
 * MARU_ENABLE_DIAGNOSTICS, and MARU_GATHER_METRICS.
 *
 * When MARU_VALIDATE_API_CALLS is enabled, invalid API usage is treated as a
 * contract violation and MARU may fail fast with abort() rather than trying to
 * return a recoverable MARU_Status.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "maru/details/maru_config.h"

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define MARU_STATIC_ASSERT _Static_assert
#elif defined(__cplusplus) && __cplusplus >= 201103L
#define MARU_STATIC_ASSERT static_assert
#else
#define MARU_STATIC_ASSERT(cond, msg)
#endif

#ifndef MARU_API
#ifdef MARU_STATIC
#define MARU_API
#elif defined(_WIN32)
#ifdef MARU_BUILDING_DLL
#define MARU_API __declspec(dllexport)
#else
#define MARU_API __declspec(dllimport)
#endif
#else
#define MARU_API __attribute__((visibility("default")))
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ----- Core vocabulary ----- */

typedef uint64_t MARU_Flags;
typedef uint64_t MARU_EventMask;

#define MARU_BIT(n) ((uint64_t)1 << (n))

/**
 * Note: MARU_Status is not meant to tell you what went wrong, but rather what you can do about it.
 * It's a control flow mechanism, not a debugging tool. Use a diagnostic callback to get root cause
 * information.
 *
 * Invoking an api method on a lost context is **NOT** an API violation, but is guaranteed to be a
 * no-op. You do not need to explictly check for MARU_ERROR_CONTEXT_LOST everywhere. Doing it on the
 * result of maru_pumpEvents() is often plenty.
 *
 * Handles owned by a lost context should be treated as inert. Status-returning
 * APIs on those child handles will typically short-circuit with
 * MARU_ERROR_CONTEXT_LOST; destroy the context rather than trying to tear down
 * attached child objects one by one.
 */
typedef enum MARU_Status {
  MARU_SUCCESS = 0,             // The operation succeeded
  MARU_FAILURE = 1,             // The operatoin failed, but the backend is still healthy
  MARU_ERROR_CONTEXT_LOST = 2,  // The context is dead and now inert. You will have to rebuild it
} MARU_Status;

/*
 * MARU_BACKEND_UNKNOWN lets MARU pick the native backend automatically.
 *
 * On Linux builds that include both backends, this prefers Wayland and falls
 * back to X11 if Wayland is unavailable. On other platforms, UNKNOWN selects
 * the platform-default native backend.
 */
typedef enum MARU_BackendType {
  MARU_BACKEND_UNKNOWN = 0,
  MARU_BACKEND_WAYLAND = 1,
  MARU_BACKEND_X11 = 2,
  MARU_BACKEND_WINDOWS = 3,
  MARU_BACKEND_COCOA = 4,
} MARU_BackendType;

typedef struct MARU_Version {
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
} MARU_Version;

typedef void* (*MARU_AllocationFunction)(size_t size, void* userdata);
typedef void* (*MARU_ReallocationFunction)(void* ptr, size_t new_size, void* userdata);
typedef void (*MARU_FreeFunction)(void* ptr, void* userdata);

typedef struct MARU_Allocator {
  MARU_AllocationFunction alloc_cb;
  MARU_ReallocationFunction realloc_cb;
  MARU_FreeFunction free_cb;
  void* userdata;
} MARU_Allocator;

typedef struct MARU_ChannelInfo {
  const char* name;
  uint32_t native_code;
  uint32_t flags;
} MARU_ChannelInfo;

#define MARU_CHANNEL_FLAG_IS_DEFAULT MARU_BIT(0)

MARU_API MARU_Version maru_getVersion(void);

/* ----- Geometry ----- */

typedef double MARU_Scalar;

typedef struct MARU_Fraction {
  int32_t num;
  int32_t denom;
} MARU_Fraction;

typedef struct MARU_Vec2Px {
  int32_t x;
  int32_t y;
} MARU_Vec2Px;

typedef struct MARU_Vec2Mm {
  MARU_Scalar x;
  MARU_Scalar y;
} MARU_Vec2Mm;

typedef struct MARU_Vec2Dip {
  MARU_Scalar x;
  MARU_Scalar y;
} MARU_Vec2Dip;

typedef struct MARU_RectDip {
  MARU_Vec2Dip dip_position;
  MARU_Vec2Dip dip_size;
} MARU_RectDip;

typedef enum MARU_BufferTransform {
  MARU_BUFFER_TRANSFORM_NORMAL = 0,
  MARU_BUFFER_TRANSFORM_90 = 1,
  MARU_BUFFER_TRANSFORM_180 = 2,
  MARU_BUFFER_TRANSFORM_270 = 3,
  MARU_BUFFER_TRANSFORM_FLIPPED = 4,
  MARU_BUFFER_TRANSFORM_FLIPPED_90 = 5,
  MARU_BUFFER_TRANSFORM_FLIPPED_180 = 6,
  MARU_BUFFER_TRANSFORM_FLIPPED_270 = 7,
} MARU_BufferTransform;

typedef struct MARU_WindowGeometry {
  MARU_Vec2Dip dip_position;
  MARU_Vec2Dip dip_size;
  MARU_Vec2Px px_size;
  MARU_Scalar scale;
  MARU_BufferTransform buffer_transform;
} MARU_WindowGeometry;

/* ----- Metrics and tuning ----- */

typedef struct MARU_UserEventMetrics {
  uint32_t peak_count;
  uint32_t current_capacity;
  uint32_t drop_count;
} MARU_UserEventMetrics;

typedef struct MARU_ContextMetrics {
  const MARU_UserEventMetrics* user_events;
  uint64_t cursor_create_count_total;
  uint64_t cursor_destroy_count_total;
  uint64_t cursor_create_count_system;
  uint64_t cursor_create_count_custom;
  uint64_t cursor_alive_current;
  uint64_t cursor_alive_peak;
  uint64_t pump_call_count_total;
  uint64_t pump_duration_avg_ns;
  uint64_t pump_duration_peak_ns;
  uint64_t memory_allocated_current;
  uint64_t memory_allocated_peak;
} MARU_ContextMetrics;

typedef struct MARU_WindowMetrics {
  void* reserved[4];
} MARU_WindowMetrics;

typedef struct MARU_MonitorMetrics {
  void* reserved[4];
} MARU_MonitorMetrics;

typedef struct MARU_CursorMetrics {
  void* reserved[4];
} MARU_CursorMetrics;

typedef struct MARU_ControllerMetrics {
  void* reserved[4];
} MARU_ControllerMetrics;

typedef enum MARU_WaylandDecorationMode {
  MARU_WAYLAND_DECORATION_MODE_AUTO = 0,
  MARU_WAYLAND_DECORATION_MODE_SSD = 1,
  MARU_WAYLAND_DECORATION_MODE_CSD = 2,
  MARU_WAYLAND_DECORATION_MODE_NONE = 3,
} MARU_WaylandDecorationMode;

typedef enum MARU_CursorPolicy {
  MARU_CURSOR_POLICY_SYSTEM_WITH_MARU_FALLBACK = 0,
  MARU_CURSOR_POLICY_SYSTEM_ONLY = 1,
  MARU_CURSOR_POLICY_MARU_ONLY = 2,
} MARU_CursorPolicy;

typedef struct MARU_ContextTuning {
  uint32_t user_event_queue_size;
  MARU_CursorPolicy cursor_policy;

  struct {
    MARU_WaylandDecorationMode decoration_mode;
  } wayland;

  struct {
    uint32_t idle_poll_interval_ms;
    uint32_t selection_query_timeout_ms;
  } x11;
} MARU_ContextTuning;

#define MARU_CONTEXT_TUNING_DEFAULT                                            \
  {                                                                            \
      .user_event_queue_size = 256,                                            \
      .cursor_policy = MARU_CURSOR_POLICY_SYSTEM_WITH_MARU_FALLBACK,           \
      .wayland = {.decoration_mode = MARU_WAYLAND_DECORATION_MODE_AUTO},       \
      .x11 = {.idle_poll_interval_ms = 250, .selection_query_timeout_ms = 50}, \
  }

/* ----- Public handles ----- */

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;
typedef struct MARU_Monitor MARU_Monitor;
typedef struct MARU_Controller MARU_Controller;
typedef struct MARU_Cursor MARU_Cursor;
typedef struct MARU_Image MARU_Image;
typedef struct MARU_Queue MARU_Queue;
typedef struct MARU_DropSession MARU_DropSession;
typedef struct MARU_DataRequest MARU_DataRequest;

/* ----- Diagnostics ----- */

typedef enum MARU_Diagnostic {
  MARU_DIAGNOSTIC_NONE = 0,
  MARU_DIAGNOSTIC_INFO,
  MARU_DIAGNOSTIC_OUT_OF_MEMORY,
  MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE,
  MARU_DIAGNOSTIC_DYNAMIC_LIB_FAILURE,
  MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED,
  MARU_DIAGNOSTIC_BACKEND_FAILURE,
  MARU_DIAGNOSTIC_BACKEND_UNAVAILABLE,
  MARU_DIAGNOSTIC_VULKAN_FAILURE,
  MARU_DIAGNOSTIC_UNKNOWN,
  MARU_DIAGNOSTIC_INVALID_ARGUMENT,
  MARU_DIAGNOSTIC_PRECONDITION_FAILURE,
  MARU_DIAGNOSTIC_INTERNAL,
} MARU_Diagnostic;

typedef struct MARU_DiagnosticInfo {
  MARU_Diagnostic diagnostic;
  const char* message;
  MARU_Context* context;
  MARU_Window* window;
} MARU_DiagnosticInfo;

typedef void (*MARU_DiagnosticCallback)(const MARU_DiagnosticInfo* info, void* userdata);

/* ----- Input model ----- */

typedef uint64_t MARU_ModifierFlags;

typedef enum MARU_ButtonState {
  MARU_BUTTON_STATE_RELEASED = 0,
  MARU_BUTTON_STATE_PRESSED = 1,
} MARU_ButtonState;

typedef uint8_t MARU_ButtonState8;

typedef enum MARU_Key {
  MARU_KEY_UNKNOWN = 0,
  MARU_KEY_SPACE,
  MARU_KEY_APOSTROPHE,
  MARU_KEY_COMMA,
  MARU_KEY_MINUS,
  MARU_KEY_PERIOD,
  MARU_KEY_SLASH,
  MARU_KEY_0,
  MARU_KEY_1,
  MARU_KEY_2,
  MARU_KEY_3,
  MARU_KEY_4,
  MARU_KEY_5,
  MARU_KEY_6,
  MARU_KEY_7,
  MARU_KEY_8,
  MARU_KEY_9,
  MARU_KEY_SEMICOLON,
  MARU_KEY_EQUAL,
  MARU_KEY_A,
  MARU_KEY_B,
  MARU_KEY_C,
  MARU_KEY_D,
  MARU_KEY_E,
  MARU_KEY_F,
  MARU_KEY_G,
  MARU_KEY_H,
  MARU_KEY_I,
  MARU_KEY_J,
  MARU_KEY_K,
  MARU_KEY_L,
  MARU_KEY_M,
  MARU_KEY_N,
  MARU_KEY_O,
  MARU_KEY_P,
  MARU_KEY_Q,
  MARU_KEY_R,
  MARU_KEY_S,
  MARU_KEY_T,
  MARU_KEY_U,
  MARU_KEY_V,
  MARU_KEY_W,
  MARU_KEY_X,
  MARU_KEY_Y,
  MARU_KEY_Z,
  MARU_KEY_LEFT_BRACKET,
  MARU_KEY_BACKSLASH,
  MARU_KEY_RIGHT_BRACKET,
  MARU_KEY_GRAVE_ACCENT,
  MARU_KEY_ESCAPE,
  MARU_KEY_ENTER,
  MARU_KEY_TAB,
  MARU_KEY_BACKSPACE,
  MARU_KEY_INSERT,
  MARU_KEY_DELETE,
  MARU_KEY_RIGHT,
  MARU_KEY_LEFT,
  MARU_KEY_DOWN,
  MARU_KEY_UP,
  MARU_KEY_PAGE_UP,
  MARU_KEY_PAGE_DOWN,
  MARU_KEY_HOME,
  MARU_KEY_END,
  MARU_KEY_CAPS_LOCK,
  MARU_KEY_SCROLL_LOCK,
  MARU_KEY_NUM_LOCK,
  MARU_KEY_PRINT_SCREEN,
  MARU_KEY_PAUSE,
  MARU_KEY_F1,
  MARU_KEY_F2,
  MARU_KEY_F3,
  MARU_KEY_F4,
  MARU_KEY_F5,
  MARU_KEY_F6,
  MARU_KEY_F7,
  MARU_KEY_F8,
  MARU_KEY_F9,
  MARU_KEY_F10,
  MARU_KEY_F11,
  MARU_KEY_F12,
  MARU_KEY_KP_0,
  MARU_KEY_KP_1,
  MARU_KEY_KP_2,
  MARU_KEY_KP_3,
  MARU_KEY_KP_4,
  MARU_KEY_KP_5,
  MARU_KEY_KP_6,
  MARU_KEY_KP_7,
  MARU_KEY_KP_8,
  MARU_KEY_KP_9,
  MARU_KEY_KP_DECIMAL,
  MARU_KEY_KP_DIVIDE,
  MARU_KEY_KP_MULTIPLY,
  MARU_KEY_KP_SUBTRACT,
  MARU_KEY_KP_ADD,
  MARU_KEY_KP_ENTER,
  MARU_KEY_KP_EQUAL,
  MARU_KEY_LEFT_SHIFT,
  MARU_KEY_LEFT_CONTROL,
  MARU_KEY_LEFT_ALT,
  MARU_KEY_LEFT_META,
  MARU_KEY_RIGHT_SHIFT,
  MARU_KEY_RIGHT_CONTROL,
  MARU_KEY_RIGHT_ALT,
  MARU_KEY_RIGHT_META,
  MARU_KEY_MENU,
  MARU_KEY_COUNT
} MARU_Key;

#define MARU_MODIFIER_SHIFT MARU_BIT(0)
#define MARU_MODIFIER_CONTROL MARU_BIT(1)
#define MARU_MODIFIER_ALT MARU_BIT(2)
#define MARU_MODIFIER_META MARU_BIT(3)
#define MARU_MODIFIER_CAPS_LOCK MARU_BIT(4)
#define MARU_MODIFIER_NUM_LOCK MARU_BIT(5)

typedef enum MARU_MouseDefaultButton {
  MARU_MOUSE_DEFAULT_LEFT = 0,
  MARU_MOUSE_DEFAULT_RIGHT = 1,
  MARU_MOUSE_DEFAULT_MIDDLE = 2,
  MARU_MOUSE_DEFAULT_BACK = 3,
  MARU_MOUSE_DEFAULT_FORWARD = 4,
  MARU_MOUSE_DEFAULT_COUNT = 5,
} MARU_MouseDefaultButton;

typedef struct MARU_AnalogInputState {
  MARU_Scalar value;
} MARU_AnalogInputState;

static inline uint32_t maru_getKeyboardKeyCount(const MARU_Context* context);
static inline const MARU_ButtonState8* maru_getKeyboardKeyStates(const MARU_Context* context);
static inline bool maru_isKeyboardKeyPressed(const MARU_Context* context, MARU_Key key);

/* ----- Event model ----- */

typedef enum MARU_EventId {
  MARU_EVENT_CLOSE_REQUESTED = 0,
  MARU_EVENT_WINDOW_RESIZED = 1,
  MARU_EVENT_KEY_CHANGED = 2,
  MARU_EVENT_WINDOW_READY = 3,
  MARU_EVENT_MOUSE_MOVED = 4,
  MARU_EVENT_MOUSE_BUTTON_CHANGED = 5,
  MARU_EVENT_MOUSE_SCROLLED = 6,
  MARU_EVENT_IDLE_CHANGED = 7,
  MARU_EVENT_MONITOR_CHANGED = 8,
  MARU_EVENT_MONITOR_MODE_CHANGED = 9,
  MARU_EVENT_WINDOW_FRAME = 10,
  MARU_EVENT_WINDOW_STATE_CHANGED = 11,
  MARU_EVENT_TEXT_EDIT_STARTED = 12,
  MARU_EVENT_TEXT_EDIT_UPDATED = 13,
  MARU_EVENT_TEXT_EDIT_COMMITTED = 14,
  MARU_EVENT_TEXT_EDIT_ENDED = 15,
  MARU_EVENT_DROP_ENTERED = 16,
  MARU_EVENT_DROP_HOVERED = 17,
  MARU_EVENT_DROP_EXITED = 18,
  MARU_EVENT_DROP_DROPPED = 19,
  MARU_EVENT_DATA_RECEIVED = 20,
  MARU_EVENT_DATA_REQUESTED = 21,
  MARU_EVENT_DATA_CONSUMED = 22,
  MARU_EVENT_DRAG_FINISHED = 23,
  MARU_EVENT_CONTROLLER_CHANGED = 24,
  MARU_EVENT_CONTROLLER_BUTTON_CHANGED = 25,
  MARU_EVENT_USER_0 = 48,
  MARU_EVENT_USER_1 = 49,
  MARU_EVENT_USER_2 = 50,
  MARU_EVENT_USER_3 = 51,
  MARU_EVENT_USER_4 = 52,
  MARU_EVENT_USER_5 = 53,
  MARU_EVENT_USER_6 = 54,
  MARU_EVENT_USER_7 = 55,
  MARU_EVENT_USER_8 = 56,
  MARU_EVENT_USER_9 = 57,
  MARU_EVENT_USER_10 = 58,
  MARU_EVENT_USER_11 = 59,
  MARU_EVENT_USER_12 = 60,
  MARU_EVENT_USER_13 = 61,
  MARU_EVENT_USER_14 = 62,
  MARU_EVENT_USER_15 = 63,
} MARU_EventId;

#define MARU_EVENT_MASK(id) MARU_BIT((uint32_t)(id))

#define MARU_MASK_CLOSE_REQUESTED MARU_EVENT_MASK(MARU_EVENT_CLOSE_REQUESTED)
#define MARU_MASK_WINDOW_RESIZED MARU_EVENT_MASK(MARU_EVENT_WINDOW_RESIZED)
#define MARU_MASK_KEY_CHANGED MARU_EVENT_MASK(MARU_EVENT_KEY_CHANGED)
#define MARU_MASK_WINDOW_READY MARU_EVENT_MASK(MARU_EVENT_WINDOW_READY)
#define MARU_MASK_MOUSE_MOVED MARU_EVENT_MASK(MARU_EVENT_MOUSE_MOVED)
#define MARU_MASK_MOUSE_BUTTON_CHANGED MARU_EVENT_MASK(MARU_EVENT_MOUSE_BUTTON_CHANGED)
#define MARU_MASK_MOUSE_SCROLLED MARU_EVENT_MASK(MARU_EVENT_MOUSE_SCROLLED)
#define MARU_MASK_IDLE_CHANGED MARU_EVENT_MASK(MARU_EVENT_IDLE_CHANGED)
#define MARU_MASK_MONITOR_CHANGED MARU_EVENT_MASK(MARU_EVENT_MONITOR_CHANGED)
#define MARU_MASK_MONITOR_MODE_CHANGED MARU_EVENT_MASK(MARU_EVENT_MONITOR_MODE_CHANGED)
#define MARU_MASK_WINDOW_FRAME MARU_EVENT_MASK(MARU_EVENT_WINDOW_FRAME)
#define MARU_MASK_WINDOW_STATE_CHANGED MARU_EVENT_MASK(MARU_EVENT_WINDOW_STATE_CHANGED)
#define MARU_MASK_TEXT_EDIT_STARTED MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_STARTED)
#define MARU_MASK_TEXT_EDIT_UPDATED MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_UPDATED)
#define MARU_MASK_TEXT_EDIT_COMMITTED MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_COMMITTED)
#define MARU_MASK_TEXT_EDIT_ENDED MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_ENDED)
#define MARU_MASK_DROP_ENTERED MARU_EVENT_MASK(MARU_EVENT_DROP_ENTERED)
#define MARU_MASK_DROP_HOVERED MARU_EVENT_MASK(MARU_EVENT_DROP_HOVERED)
#define MARU_MASK_DROP_EXITED MARU_EVENT_MASK(MARU_EVENT_DROP_EXITED)
#define MARU_MASK_DROP_DROPPED MARU_EVENT_MASK(MARU_EVENT_DROP_DROPPED)
#define MARU_MASK_DATA_RECEIVED MARU_EVENT_MASK(MARU_EVENT_DATA_RECEIVED)
#define MARU_MASK_DATA_REQUESTED MARU_EVENT_MASK(MARU_EVENT_DATA_REQUESTED)
#define MARU_MASK_DATA_CONSUMED MARU_EVENT_MASK(MARU_EVENT_DATA_CONSUMED)
#define MARU_MASK_DRAG_FINISHED MARU_EVENT_MASK(MARU_EVENT_DRAG_FINISHED)
#define MARU_MASK_CONTROLLER_CHANGED MARU_EVENT_MASK(MARU_EVENT_CONTROLLER_CHANGED)
#define MARU_MASK_CONTROLLER_BUTTON_CHANGED MARU_EVENT_MASK(MARU_EVENT_CONTROLLER_BUTTON_CHANGED)
#define MARU_MASK_USER_0 MARU_EVENT_MASK(MARU_EVENT_USER_0)
#define MARU_MASK_USER_1 MARU_EVENT_MASK(MARU_EVENT_USER_1)
#define MARU_MASK_USER_2 MARU_EVENT_MASK(MARU_EVENT_USER_2)
#define MARU_MASK_USER_3 MARU_EVENT_MASK(MARU_EVENT_USER_3)
#define MARU_MASK_USER_4 MARU_EVENT_MASK(MARU_EVENT_USER_4)
#define MARU_MASK_USER_5 MARU_EVENT_MASK(MARU_EVENT_USER_5)
#define MARU_MASK_USER_6 MARU_EVENT_MASK(MARU_EVENT_USER_6)
#define MARU_MASK_USER_7 MARU_EVENT_MASK(MARU_EVENT_USER_7)
#define MARU_MASK_USER_8 MARU_EVENT_MASK(MARU_EVENT_USER_8)
#define MARU_MASK_USER_9 MARU_EVENT_MASK(MARU_EVENT_USER_9)
#define MARU_MASK_USER_10 MARU_EVENT_MASK(MARU_EVENT_USER_10)
#define MARU_MASK_USER_11 MARU_EVENT_MASK(MARU_EVENT_USER_11)
#define MARU_MASK_USER_12 MARU_EVENT_MASK(MARU_EVENT_USER_12)
#define MARU_MASK_USER_13 MARU_EVENT_MASK(MARU_EVENT_USER_13)
#define MARU_MASK_USER_14 MARU_EVENT_MASK(MARU_EVENT_USER_14)
#define MARU_MASK_USER_15 MARU_EVENT_MASK(MARU_EVENT_USER_15)
#define MARU_ALL_EVENTS (~((MARU_EventMask)0))

static inline bool maru_isUserEventId(MARU_EventId id) {
  return id >= MARU_EVENT_USER_0 && id <= MARU_EVENT_USER_15;
}

static inline bool maru_isKnownEventId(MARU_EventId id) {
  switch (id) {
    case MARU_EVENT_CLOSE_REQUESTED:
    case MARU_EVENT_WINDOW_RESIZED:
    case MARU_EVENT_KEY_CHANGED:
    case MARU_EVENT_WINDOW_READY:
    case MARU_EVENT_MOUSE_MOVED:
    case MARU_EVENT_MOUSE_BUTTON_CHANGED:
    case MARU_EVENT_MOUSE_SCROLLED:
    case MARU_EVENT_IDLE_CHANGED:
    case MARU_EVENT_MONITOR_CHANGED:
    case MARU_EVENT_MONITOR_MODE_CHANGED:
    case MARU_EVENT_WINDOW_FRAME:
    case MARU_EVENT_WINDOW_STATE_CHANGED:
    case MARU_EVENT_TEXT_EDIT_STARTED:
    case MARU_EVENT_TEXT_EDIT_UPDATED:
    case MARU_EVENT_TEXT_EDIT_COMMITTED:
    case MARU_EVENT_TEXT_EDIT_ENDED:
    case MARU_EVENT_DROP_ENTERED:
    case MARU_EVENT_DROP_HOVERED:
    case MARU_EVENT_DROP_EXITED:
    case MARU_EVENT_DROP_DROPPED:
    case MARU_EVENT_DATA_RECEIVED:
    case MARU_EVENT_DATA_REQUESTED:
    case MARU_EVENT_DATA_CONSUMED:
    case MARU_EVENT_DRAG_FINISHED:
    case MARU_EVENT_CONTROLLER_CHANGED:
    case MARU_EVENT_CONTROLLER_BUTTON_CHANGED:
      return true;
    default:
      return maru_isUserEventId(id);
  }
}

static inline bool maru_eventMaskHas(MARU_EventMask mask, MARU_EventId id) {
  return (mask & MARU_EVENT_MASK(id)) != 0;
}

static inline MARU_EventMask maru_eventMaskAdd(MARU_EventMask mask, MARU_EventId id) {
  return mask | MARU_EVENT_MASK(id);
}

static inline MARU_EventMask maru_eventMaskRemove(MARU_EventMask mask, MARU_EventId id) {
  return mask & ~MARU_EVENT_MASK(id);
}

typedef struct MARU_WindowReadyEvent {
  MARU_WindowGeometry geometry;
} MARU_WindowReadyEvent;

typedef struct MARU_CloseRequestedEvent {
  char _unused;
} MARU_CloseRequestedEvent;

typedef struct MARU_WindowFrameEvent {
  uint32_t timestamp_ms;
} MARU_WindowFrameEvent;

typedef struct MARU_WindowResizedEvent {
  MARU_WindowGeometry geometry;
} MARU_WindowResizedEvent;

typedef enum MARU_WindowStateChangedBits {
  MARU_WINDOW_STATE_CHANGED_VISIBLE = MARU_BIT(0),
  MARU_WINDOW_STATE_CHANGED_MINIMIZED = MARU_BIT(1),
  MARU_WINDOW_STATE_CHANGED_MAXIMIZED = MARU_BIT(2),
  MARU_WINDOW_STATE_CHANGED_FOCUSED = MARU_BIT(3),
  MARU_WINDOW_STATE_CHANGED_ICON = MARU_BIT(4),
} MARU_WindowStateChangedBits;

typedef struct MARU_WindowStateChangedEvent {
  uint32_t changed_fields;
  bool visible;
  bool minimized;
  bool maximized;
  bool focused;
  bool icon_changed;
} MARU_WindowStateChangedEvent;

typedef struct MARU_KeyChangedEvent {
  MARU_Key raw_key;
  MARU_ButtonState state;
  MARU_ModifierFlags modifiers;
} MARU_KeyChangedEvent;

typedef struct MARU_MouseMovedEvent {
  MARU_Vec2Dip dip_position;
  MARU_Vec2Dip dip_delta;
  MARU_Vec2Dip raw_dip_delta;
  MARU_ModifierFlags modifiers;
} MARU_MouseMovedEvent;

typedef struct MARU_MouseButtonChangedEvent {
  uint32_t button_id;
  MARU_ButtonState state;
  MARU_ModifierFlags modifiers;
} MARU_MouseButtonChangedEvent;

typedef struct MARU_MouseScrolledEvent {
  MARU_Vec2Dip dip_delta;
  struct {
    int32_t x;
    int32_t y;
  } steps;
  MARU_ModifierFlags modifiers;
} MARU_MouseScrolledEvent;

typedef struct MARU_IdleChangedEvent {
  uint32_t timeout_ms;
  bool is_idle;
} MARU_IdleChangedEvent;

typedef struct MARU_MonitorChangedEvent {
  /* Transient handle. Retain it if it must outlive the current pump cycle. */
  MARU_Monitor* monitor;
  bool connected;
} MARU_MonitorChangedEvent;

typedef struct MARU_MonitorModeChangedEvent {
  /* Transient handle. Retain it if it must outlive the current pump cycle. */
  MARU_Monitor* monitor;
} MARU_MonitorModeChangedEvent;

typedef enum MARU_DropAction {
  MARU_DROP_ACTION_NONE = 0,
  MARU_DROP_ACTION_COPY = MARU_BIT(0),
  MARU_DROP_ACTION_MOVE = MARU_BIT(1),
  MARU_DROP_ACTION_LINK = MARU_BIT(2),
} MARU_DropAction;

typedef uint64_t MARU_DropActionMask;

typedef enum MARU_DataExchangeTarget {
  MARU_DATA_EXCHANGE_TARGET_CLIPBOARD = 0,
  MARU_DATA_EXCHANGE_TARGET_PRIMARY = 1,
  MARU_DATA_EXCHANGE_TARGET_DRAG_DROP = 2,
} MARU_DataExchangeTarget;

typedef struct MARU_MIMETypeList {
  const char* const* mime_types;
  uint32_t count;
} MARU_MIMETypeList;

typedef enum MARU_DataProvideFlags {
  MARU_DATA_PROVIDE_FLAG_NONE = 0,
  MARU_DATA_PROVIDE_FLAG_ZERO_COPY = MARU_BIT(0),
} MARU_DataProvideFlags;

typedef struct MARU_DropEnteredEvent {
  MARU_Vec2Dip dip_position;
  MARU_DropSession* session;
  /* Borrowed callback-scoped pointers. Copy what you need before returning. */
  const char** paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint32_t path_count;
} MARU_DropEnteredEvent;

typedef struct MARU_DropHoveredEvent {
  MARU_Vec2Dip dip_position;
  MARU_DropSession* session;
  /* Borrowed callback-scoped pointers. Copy what you need before returning. */
  const char** paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint32_t path_count;
} MARU_DropHoveredEvent;

typedef struct MARU_DropExitedEvent {
  MARU_DropSession* session;
} MARU_DropExitedEvent;

typedef struct MARU_DropDroppedEvent {
  MARU_Vec2Dip dip_position;
  MARU_DropSession* session;
  /* Borrowed callback-scoped pointers. Copy what you need before returning. */
  const char** paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint32_t path_count;
} MARU_DropDroppedEvent;

typedef struct MARU_DataReceivedEvent {
  void* userdata;
  MARU_Status status;
  MARU_DataExchangeTarget target;
  /* Borrowed callback-scoped pointers. Copy what you need before returning. */
  const char* mime_type;
  const void* data;
  size_t size;
} MARU_DataReceivedEvent;

typedef struct MARU_DataRequestEvent {
  MARU_DataExchangeTarget target;
  /*
   * `request` and `mime_type` are only valid for the duration of the
   * MARU_EVENT_DATA_REQUESTED callback that delivered them.
   */
  const char* mime_type;
  MARU_DataRequest* request;
} MARU_DataRequestEvent;

typedef struct MARU_DataConsumedEvent {
  MARU_DataExchangeTarget target;
  /* Borrowed callback-scoped pointers. Copy what you need before returning. */
  const char* mime_type;
  const void* data;
  size_t size;
  MARU_DropAction action;
} MARU_DataConsumedEvent;

typedef struct MARU_DragFinishedEvent {
  MARU_DropAction action;
} MARU_DragFinishedEvent;

typedef struct MARU_ControllerChangedEvent {
  /* Transient handle. Retain it if it must outlive the current pump cycle. */
  MARU_Controller* controller;
  bool connected;
} MARU_ControllerChangedEvent;

typedef struct MARU_ControllerButtonChangedEvent {
  /* Transient handle. Retain it if it must outlive the current pump cycle. */
  MARU_Controller* controller;
  MARU_ButtonState state;
  uint32_t button_id;
} MARU_ControllerButtonChangedEvent;

typedef struct MARU_TextRangeUtf8 {
  uint32_t start_byte;
  uint32_t length_byte;
} MARU_TextRangeUtf8;

typedef struct MARU_TextEditStartedEvent {
  uint64_t session_id;
} MARU_TextEditStartedEvent;

typedef struct MARU_TextEditUpdatedEvent {
  uint64_t session_id;
  /* Borrowed callback-scoped UTF-8. Copy what you need before returning. */
  const char* preedit_utf8;
  uint32_t preedit_length;
  MARU_TextRangeUtf8 caret;
  MARU_TextRangeUtf8 selection;
} MARU_TextEditUpdatedEvent;

typedef struct MARU_TextEditCommittedEvent {
  uint64_t session_id;
  uint32_t delete_before_bytes;
  uint32_t delete_after_bytes;
  /* Borrowed callback-scoped UTF-8. Copy what you need before returning. */
  const char* committed_utf8;
  uint32_t committed_length;
} MARU_TextEditCommittedEvent;

typedef struct MARU_TextEditEndedEvent {
  uint64_t session_id;
  bool canceled;
} MARU_TextEditEndedEvent;

typedef struct MARU_UserDefinedEvent {
  union {
    void* userdata;
    char raw_payload[64];
  };
} MARU_UserDefinedEvent;

typedef struct MARU_Event {
  union {
    MARU_WindowReadyEvent window_ready;
    MARU_CloseRequestedEvent close_requested;
    MARU_WindowResizedEvent resized;
    MARU_WindowStateChangedEvent state_changed;
    MARU_KeyChangedEvent key_changed;
    MARU_MouseMovedEvent mouse_moved;
    MARU_MouseButtonChangedEvent mouse_button_changed;
    MARU_MouseScrolledEvent mouse_scrolled;
    MARU_IdleChangedEvent idle_changed;
    MARU_MonitorChangedEvent monitor_changed;
    MARU_MonitorModeChangedEvent monitor_mode_changed;
    MARU_DropEnteredEvent drop_entered;
    MARU_DropHoveredEvent drop_hovered;
    MARU_DropExitedEvent drop_exited;
    MARU_DropDroppedEvent drop_dropped;
    MARU_DataReceivedEvent data_received;
    MARU_DataRequestEvent data_requested;
    MARU_DataConsumedEvent data_consumed;
    MARU_DragFinishedEvent drag_finished;
    MARU_ControllerChangedEvent controller_changed;
    MARU_ControllerButtonChangedEvent controller_button_changed;
    MARU_WindowFrameEvent frame;
    MARU_TextEditStartedEvent text_edit_started;
    MARU_TextEditUpdatedEvent text_edit_updated;
    MARU_TextEditCommittedEvent text_edit_committed;
    MARU_TextEditEndedEvent text_edit_ended;
    MARU_UserDefinedEvent user;
  };
} MARU_Event;

MARU_STATIC_ASSERT(sizeof(MARU_Event) <= 64, "MARU_Event ABI Violation: Size exceeds 64 bytes.");
MARU_STATIC_ASSERT(sizeof(MARU_UserDefinedEvent) == 64,
                   "MARU_UserDefinedEvent ABI Violation: Expected 64 bytes.");

/*
 * Direct pump callback. Matching events are delivered synchronously and inline
 * from maru_pumpEvents().
 *
 * `window` is NULL for context-scoped events such as monitor/controller
 * changes, and for posted user events that do not target a window.
 *
 * Any borrowed pointer reachable through `evt` is only guaranteed to remain
 * valid for the duration of the callback unless a specific event says
 * otherwise.
 */
typedef void (*MARU_EventCallback)(MARU_EventId type,
                                   MARU_Window* window,
                                   const MARU_Event* evt,
                                   void* userdata);

/*
 * Pumps backend events and synchronously dispatches matching callbacks inline.
 *
 * User events posted with maru_postEvent() are drained near the start of a
 * pump. Events posted from inside a callback are queued for a later pump and
 * are never delivered reentrantly from the same maru_pumpEvents() call.
 */
MARU_API MARU_Status maru_pumpEvents(MARU_Context* context,
                                     uint32_t timeout_ms,
                                     MARU_EventMask mask,
                                     MARU_EventCallback callback,
                                     void* userdata);
/* Thread-safe user-event injection for a later pump cycle. */
MARU_API MARU_Status maru_postEvent(MARU_Context* context,
                                    MARU_EventId type,
                                    MARU_Window* window,
                                    MARU_UserDefinedEvent evt);
MARU_API MARU_Status maru_wakeContext(MARU_Context* context);

/* ----- Contexts ----- */

#define MARU_CONTEXT_STATE_LOST MARU_BIT(0)
#define MARU_CONTEXT_STATE_READY MARU_BIT(1)

#define MARU_NEVER UINT32_MAX
#define MARU_CONTEXT_ATTR_INHIBIT_IDLE MARU_BIT(0)
#define MARU_CONTEXT_ATTR_DIAGNOSTICS MARU_BIT(1)
#define MARU_CONTEXT_ATTR_DEFAULT_CURSOR MARU_BIT(2)
#define MARU_CONTEXT_ATTR_IDLE_TIMEOUT MARU_BIT(3)
#define MARU_CONTEXT_ATTR_ALL                                       \
  (MARU_CONTEXT_ATTR_INHIBIT_IDLE | MARU_CONTEXT_ATTR_DIAGNOSTICS | \
   MARU_CONTEXT_ATTR_DEFAULT_CURSOR | MARU_CONTEXT_ATTR_IDLE_TIMEOUT)

static inline void* maru_getContextUserdata(const MARU_Context* context);
static inline void maru_setContextUserdata(MARU_Context* context, void* userdata);
static inline bool maru_isContextLost(const MARU_Context* context);
static inline bool maru_isContextReady(const MARU_Context* context);
static inline MARU_BackendType maru_getContextBackend(const MARU_Context* context);
static inline const MARU_ContextMetrics* maru_getContextMetrics(const MARU_Context* context);
static inline uint32_t maru_getContextMouseButtonCount(const MARU_Context* context);
static inline const MARU_ButtonState8* maru_getContextMouseButtonStates(
    const MARU_Context* context);
static inline const MARU_ChannelInfo* maru_getContextMouseButtonChannelInfo(
    const MARU_Context* context);
static inline int32_t maru_getContextMouseDefaultButtonChannel(const MARU_Context* context,
                                                               MARU_MouseDefaultButton which);
static inline bool maru_isContextMouseButtonPressed(const MARU_Context* context,
                                                    uint32_t button_id);

typedef struct MARU_ContextAttributes {
  MARU_DiagnosticCallback diagnostic_cb;
  void* diagnostic_userdata;
  MARU_Cursor* default_cursor;
  bool inhibit_idle;
  uint32_t idle_timeout_ms;
} MARU_ContextAttributes;

typedef struct MARU_ContextCreateInfo {
  MARU_Allocator allocator;
  MARU_BackendType backend;
  MARU_ContextAttributes attributes;
  MARU_ContextTuning tuning;
  void* userdata;
} MARU_ContextCreateInfo;

#define MARU_CONTEXT_CREATE_INFO_DEFAULT                                                      \
  {                                                                                           \
      .allocator = {.alloc_cb = NULL, .realloc_cb = NULL, .free_cb = NULL, .userdata = NULL}, \
      .backend = MARU_BACKEND_UNKNOWN,                                                        \
      .attributes = {.diagnostic_cb = NULL,                                                   \
                     .diagnostic_userdata = NULL,                                             \
                     .default_cursor = NULL,                                                  \
                     .inhibit_idle = false,                                                   \
                     .idle_timeout_ms = 0},                                                   \
      .tuning = MARU_CONTEXT_TUNING_DEFAULT,                                                  \
      .userdata = NULL,                                                                       \
  }

MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo* create_info,
                                        MARU_Context** out_context);
/*
 * Destroys the context and tears down any child objects still attached to it.
 *
 * You do not need to destroy windows, cursors, images, queues, or other
 * context-owned child objects one by one before destroying the context.
 */
MARU_API MARU_Status maru_destroyContext(MARU_Context* context);
MARU_API MARU_Status maru_updateContext(MARU_Context* context,
                                        uint64_t field_mask,
                                        const MARU_ContextAttributes* attributes);
MARU_API MARU_Status maru_resetContextMetrics(MARU_Context* context);

/* ----- Images ----- */

typedef struct MARU_ImageCreateInfo {
  MARU_Vec2Px px_size;
  /* Copied into MARU-owned storage before maru_createImage() returns. */
  const uint32_t* pixels;
  uint32_t stride_bytes;
  void* userdata;
} MARU_ImageCreateInfo;

MARU_API MARU_Status maru_createImage(MARU_Context* context,
                                      const MARU_ImageCreateInfo* create_info,
                                      MARU_Image** out_image);
MARU_API MARU_Status maru_destroyImage(MARU_Image* image);
static inline void* maru_getImageUserdata(const MARU_Image* image);
static inline void maru_setImageUserdata(MARU_Image* image, void* userdata);

/* ----- Cursors ----- */

#define MARU_CURSOR_FLAG_SYSTEM MARU_BIT(0)

typedef enum MARU_CursorSource {
  MARU_CURSOR_SOURCE_SYSTEM = 0,
  MARU_CURSOR_SOURCE_CUSTOM = 1,
} MARU_CursorSource;

typedef enum MARU_CursorShape {
  MARU_CURSOR_SHAPE_DEFAULT = 0,
  MARU_CURSOR_SHAPE_HELP = 1,
  MARU_CURSOR_SHAPE_HAND = 2,
  MARU_CURSOR_SHAPE_WAIT = 3,
  MARU_CURSOR_SHAPE_CROSSHAIR = 4,
  MARU_CURSOR_SHAPE_TEXT = 5,
  MARU_CURSOR_SHAPE_MOVE = 6,
  MARU_CURSOR_SHAPE_NOT_ALLOWED = 7,
  MARU_CURSOR_SHAPE_EW_RESIZE = 8,
  MARU_CURSOR_SHAPE_NS_RESIZE = 9,
  MARU_CURSOR_SHAPE_NESW_RESIZE = 10,
  MARU_CURSOR_SHAPE_NWSE_RESIZE = 11,
} MARU_CursorShape;

static inline void* maru_getCursorUserdata(const MARU_Cursor* cursor);
static inline void maru_setCursorUserdata(MARU_Cursor* cursor, void* userdata);
static inline bool maru_isCursorSystem(const MARU_Cursor* cursor);
static inline const MARU_CursorMetrics* maru_getCursorMetrics(const MARU_Cursor* cursor);

typedef struct MARU_CursorFrame {
  const MARU_Image* image;
  MARU_Vec2Px px_hot_spot;
  uint32_t delay_ms;
} MARU_CursorFrame;

typedef struct MARU_CursorCreateInfo {
  MARU_CursorSource source;
  MARU_CursorShape system_shape;
  /*
   * For custom cursors, the referenced image pixels are copied into
   * cursor-owned frames during maru_createCursor().
   */
  const MARU_CursorFrame* frames;
  uint32_t frame_count;
  void* userdata;
} MARU_CursorCreateInfo;

MARU_API MARU_Status maru_createCursor(MARU_Context* context,
                                       const MARU_CursorCreateInfo* create_info,
                                       MARU_Cursor** out_cursor);
MARU_API MARU_Status maru_destroyCursor(MARU_Cursor* cursor);
MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor* cursor);

/* ----- Monitors ----- */

#define MARU_MONITOR_STATE_LOST MARU_BIT(0)

typedef struct MARU_VideoMode {
  MARU_Vec2Px px_size;
  uint32_t refresh_rate_millihz;
} MARU_VideoMode;

typedef struct MARU_VideoModeList {
  const MARU_VideoMode* modes;
  uint32_t count;
} MARU_VideoModeList;

typedef struct MARU_MonitorList {
  /*
   * Borrowed snapshot valid until the next maru_pumpEvents() call on the same
   * context. Retain handles you need to keep beyond that pump cycle.
   */
  MARU_Monitor* const* monitors;
  uint32_t count;
} MARU_MonitorList;

static inline void* maru_getMonitorUserdata(const MARU_Monitor* monitor);
static inline void maru_setMonitorUserdata(MARU_Monitor* monitor, void* userdata);
static inline MARU_Context* maru_getMonitorContext(const MARU_Monitor* monitor);
static inline bool maru_isMonitorLost(const MARU_Monitor* monitor);
static inline const MARU_MonitorMetrics* maru_getMonitorMetrics(const MARU_Monitor* monitor);
static inline const char* maru_getMonitorName(const MARU_Monitor* monitor);
static inline MARU_Vec2Mm maru_getMonitorPhysicalSize(const MARU_Monitor* monitor);
static inline MARU_VideoMode maru_getMonitorCurrentMode(const MARU_Monitor* monitor);
static inline MARU_Vec2Dip maru_getMonitorDipPosition(const MARU_Monitor* monitor);
static inline MARU_Vec2Dip maru_getMonitorDipSize(const MARU_Monitor* monitor);
static inline bool maru_isMonitorPrimary(const MARU_Monitor* monitor);
static inline MARU_Scalar maru_getMonitorScale(const MARU_Monitor* monitor);

MARU_API MARU_Status maru_getMonitors(MARU_Context* context, MARU_MonitorList* out_list);
MARU_API void maru_retainMonitor(MARU_Monitor* monitor);
MARU_API void maru_releaseMonitor(MARU_Monitor* monitor);
MARU_API MARU_Status maru_getMonitorModes(const MARU_Monitor* monitor,
                                          MARU_VideoModeList* out_list);
MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor* monitor, MARU_VideoMode mode);
MARU_API MARU_Status maru_resetMonitorMetrics(MARU_Monitor* monitor);

/* ----- Windows ----- */

#define MARU_WINDOW_STATE_LOST MARU_BIT(0)
#define MARU_WINDOW_STATE_READY MARU_BIT(1)
#define MARU_WINDOW_STATE_FOCUSED MARU_BIT(2)
#define MARU_WINDOW_STATE_MAXIMIZED MARU_BIT(3)
#define MARU_WINDOW_STATE_FULLSCREEN MARU_BIT(4)
#define MARU_WINDOW_STATE_RESIZABLE MARU_BIT(5)
#define MARU_WINDOW_STATE_DECORATED MARU_BIT(6)
#define MARU_WINDOW_STATE_VISIBLE MARU_BIT(7)
#define MARU_WINDOW_STATE_MINIMIZED MARU_BIT(8)

typedef enum MARU_CursorMode {
  MARU_CURSOR_NORMAL = 0,
  MARU_CURSOR_HIDDEN = 1,
  MARU_CURSOR_LOCKED = 2,
} MARU_CursorMode;

typedef enum MARU_ContentType {
  MARU_CONTENT_TYPE_NONE = 0,
  MARU_CONTENT_TYPE_PHOTO = 1,
  MARU_CONTENT_TYPE_VIDEO = 2,
  MARU_CONTENT_TYPE_GAME = 3,
} MARU_ContentType;

typedef enum MARU_TextInputType {
  MARU_TEXT_INPUT_TYPE_NONE = 0,
  MARU_TEXT_INPUT_TYPE_TEXT,
  MARU_TEXT_INPUT_TYPE_PASSWORD,
  MARU_TEXT_INPUT_TYPE_EMAIL,
  MARU_TEXT_INPUT_TYPE_NUMERIC,
} MARU_TextInputType;

static inline void* maru_getWindowUserdata(const MARU_Window* window);
static inline void maru_setWindowUserdata(MARU_Window* window, void* userdata);
static inline MARU_Context* maru_getWindowContext(const MARU_Window* window);
static inline const char* maru_getWindowTitle(const MARU_Window* window);
static inline bool maru_isWindowLost(const MARU_Window* window);
static inline bool maru_isWindowReady(const MARU_Window* window);
static inline bool maru_isWindowFocused(const MARU_Window* window);
static inline bool maru_isWindowMaximized(const MARU_Window* window);
static inline bool maru_isWindowFullscreen(const MARU_Window* window);
static inline bool maru_isWindowVisible(const MARU_Window* window);
static inline bool maru_isWindowMinimized(const MARU_Window* window);
static inline bool maru_isWindowResizable(const MARU_Window* window);
static inline bool maru_isWindowDecorated(const MARU_Window* window);
static inline MARU_CursorMode maru_getWindowCursorMode(const MARU_Window* window);
static inline const MARU_Image* maru_getWindowIcon(const MARU_Window* window);
static inline const MARU_WindowMetrics* maru_getWindowMetrics(const MARU_Window* window);
static inline MARU_WindowGeometry maru_getWindowGeometry(const MARU_Window* window);

#define MARU_WINDOW_ATTR_TITLE MARU_BIT(0)
#define MARU_WINDOW_ATTR_DIP_SIZE MARU_BIT(1)
#define MARU_WINDOW_ATTR_FULLSCREEN MARU_BIT(2)
#define MARU_WINDOW_ATTR_CURSOR_MODE MARU_BIT(3)
#define MARU_WINDOW_ATTR_CURSOR MARU_BIT(4)
#define MARU_WINDOW_ATTR_MONITOR MARU_BIT(5)
#define MARU_WINDOW_ATTR_MAXIMIZED MARU_BIT(6)
#define MARU_WINDOW_ATTR_MIN_DIP_SIZE MARU_BIT(7)
#define MARU_WINDOW_ATTR_MAX_DIP_SIZE MARU_BIT(8)
#define MARU_WINDOW_ATTR_DIP_POSITION MARU_BIT(9)
#define MARU_WINDOW_ATTR_ASPECT_RATIO MARU_BIT(10)
#define MARU_WINDOW_ATTR_RESIZABLE MARU_BIT(11)
#define MARU_WINDOW_ATTR_ACCEPT_DROP MARU_BIT(12)
#define MARU_WINDOW_ATTR_TEXT_INPUT_TYPE MARU_BIT(13)
#define MARU_WINDOW_ATTR_TEXT_INPUT_RECT MARU_BIT(14)
#define MARU_WINDOW_ATTR_PRIMARY_SELECTION MARU_BIT(15)
#define MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE MARU_BIT(16)
#define MARU_WINDOW_ATTR_VIEWPORT_DIP_SIZE MARU_BIT(17)
#define MARU_WINDOW_ATTR_SURROUNDING_TEXT MARU_BIT(18)
#define MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE MARU_BIT(19)
#define MARU_WINDOW_ATTR_VISIBLE MARU_BIT(20)
#define MARU_WINDOW_ATTR_MINIMIZED MARU_BIT(21)
#define MARU_WINDOW_ATTR_ICON MARU_BIT(22)

#define MARU_WINDOW_ATTR_ALL                                                                    \
  (MARU_WINDOW_ATTR_TITLE | MARU_WINDOW_ATTR_DIP_SIZE | MARU_WINDOW_ATTR_FULLSCREEN |           \
   MARU_WINDOW_ATTR_CURSOR_MODE | MARU_WINDOW_ATTR_CURSOR | MARU_WINDOW_ATTR_MONITOR |          \
   MARU_WINDOW_ATTR_MAXIMIZED | MARU_WINDOW_ATTR_MIN_DIP_SIZE | MARU_WINDOW_ATTR_MAX_DIP_SIZE | \
   MARU_WINDOW_ATTR_DIP_POSITION | MARU_WINDOW_ATTR_ASPECT_RATIO | MARU_WINDOW_ATTR_RESIZABLE | \
   MARU_WINDOW_ATTR_ACCEPT_DROP | MARU_WINDOW_ATTR_TEXT_INPUT_TYPE |                            \
   MARU_WINDOW_ATTR_TEXT_INPUT_RECT | MARU_WINDOW_ATTR_PRIMARY_SELECTION |                      \
   MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE | MARU_WINDOW_ATTR_VIEWPORT_DIP_SIZE |              \
   MARU_WINDOW_ATTR_SURROUNDING_TEXT | MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE |               \
   MARU_WINDOW_ATTR_VISIBLE | MARU_WINDOW_ATTR_MINIMIZED | MARU_WINDOW_ATTR_ICON)

/*
 * `title` and `surrounding_text` are copied by maru_createWindow() and
 * maru_updateWindow().
 *
 * `icon`, `cursor`, and `monitor` are borrowed same-context handles. They are
 * not retained by the window.
 */
typedef struct MARU_WindowAttributes {
  const char* title;
  MARU_Image* icon;

  MARU_Vec2Dip dip_size;
  MARU_Vec2Dip dip_position;
  MARU_Vec2Dip viewport_dip_size;
  bool fullscreen;
  MARU_Monitor* monitor;
  MARU_Vec2Dip min_dip_size;
  MARU_Vec2Dip max_dip_size;
  MARU_Fraction aspect_ratio;

  bool visible;
  bool minimized;
  bool maximized;
  bool resizable;
  bool accept_drop;

  MARU_CursorMode cursor_mode;
  MARU_Cursor* cursor;

  MARU_TextInputType text_input_type;
  MARU_RectDip text_input_rect;
  bool primary_selection;
  const char* surrounding_text;
  uint32_t surrounding_cursor_byte;
  uint32_t surrounding_anchor_byte;
} MARU_WindowAttributes;

typedef struct MARU_WindowCreateInfo {
  MARU_WindowAttributes attributes;
  /* Consumed during maru_createWindow(); it need only outlive that call. */
  const char* app_id;
  MARU_ContentType content_type;
  bool decorated;
  void* userdata;
} MARU_WindowCreateInfo;

#define MARU_WINDOW_CREATE_INFO_DEFAULT                         \
  {.attributes = {.title = "MARU Window",                       \
                  .icon = NULL,                                 \
                                                                \
                  .dip_size = {800, 600},                       \
                  .dip_position = {0, 0},                       \
                  .viewport_dip_size = {0, 0},                  \
                  .fullscreen = false,                          \
                  .monitor = NULL,                              \
                  .min_dip_size = {0, 0},                       \
                  .max_dip_size = {0, 0},                       \
                  .aspect_ratio = {0, 0},                       \
                                                                \
                  .visible = true,                              \
                  .minimized = false,                           \
                  .maximized = false,                           \
                  .resizable = true,                            \
                  .accept_drop = false,                         \
                                                                \
                  .cursor_mode = MARU_CURSOR_NORMAL,            \
                  .cursor = NULL,                               \
                                                                \
                  .text_input_type = MARU_TEXT_INPUT_TYPE_NONE, \
                  .text_input_rect = {{0, 0}, {0, 0}},          \
                  .primary_selection = true,                    \
                  .surrounding_text = NULL,                     \
                  .surrounding_cursor_byte = 0,                 \
                  .surrounding_anchor_byte = 0},                \
   .app_id = "maru.app",                                        \
   .content_type = MARU_CONTENT_TYPE_NONE,                      \
   .decorated = true,                                           \
   .userdata = NULL}

/*
 * The returned handle may exist before the native window is ready for use.
 * Wait for MARU_EVENT_WINDOW_READY or maru_isWindowReady(window) before using
 * APIs that require a realized native window, such as maru_updateWindow(),
 * maru_requestWindowFocus(), maru_requestWindowFrame(),
 * maru_requestWindowAttention(), data exchange APIs, native window getters,
 * and maru_createVkSurface().
 */
MARU_API MARU_Status maru_createWindow(MARU_Context* context,
                                       const MARU_WindowCreateInfo* create_info,
                                       MARU_Window** out_window);
MARU_API MARU_Status maru_destroyWindow(MARU_Window* window);
MARU_API MARU_Status maru_updateWindow(MARU_Window* window,
                                       uint64_t field_mask,
                                       const MARU_WindowAttributes* attributes);
MARU_API MARU_Status maru_requestWindowFocus(MARU_Window* window);
/* Backends may coalesce redundant requests; treat this as an edge-triggered hint. */
MARU_API MARU_Status maru_requestWindowFrame(MARU_Window* window);
MARU_API MARU_Status maru_requestWindowAttention(MARU_Window* window);
MARU_API MARU_Status maru_resetWindowMetrics(MARU_Window* window);

/* ----- Controllers ----- */

#define MARU_CONTROLLER_STATE_LOST MARU_BIT(0)

typedef struct MARU_ControllerInfo {
  const char* name;
  uint16_t vendor_id;
  uint16_t product_id;
  uint16_t version;
  uint8_t guid[16];
  bool is_standardized;
} MARU_ControllerInfo;

typedef enum MARU_ControllerAnalog {
  MARU_CONTROLLER_ANALOG_LEFT_X = 0,
  MARU_CONTROLLER_ANALOG_LEFT_Y = 1,
  MARU_CONTROLLER_ANALOG_RIGHT_X = 2,
  MARU_CONTROLLER_ANALOG_RIGHT_Y = 3,
  MARU_CONTROLLER_ANALOG_LEFT_TRIGGER = 4,
  MARU_CONTROLLER_ANALOG_RIGHT_TRIGGER = 5,
  MARU_CONTROLLER_ANALOG_STANDARD_COUNT = 6
} MARU_ControllerAnalog;

typedef enum MARU_ControllerButton {
  MARU_CONTROLLER_BUTTON_SOUTH = 0,
  MARU_CONTROLLER_BUTTON_EAST = 1,
  MARU_CONTROLLER_BUTTON_WEST = 2,
  MARU_CONTROLLER_BUTTON_NORTH = 3,
  MARU_CONTROLLER_BUTTON_LB = 4,
  MARU_CONTROLLER_BUTTON_RB = 5,
  MARU_CONTROLLER_BUTTON_BACK = 6,
  MARU_CONTROLLER_BUTTON_START = 7,
  MARU_CONTROLLER_BUTTON_GUIDE = 8,
  MARU_CONTROLLER_BUTTON_L_THUMB = 9,
  MARU_CONTROLLER_BUTTON_R_THUMB = 10,
  MARU_CONTROLLER_BUTTON_DPAD_UP = 11,
  MARU_CONTROLLER_BUTTON_DPAD_RIGHT = 12,
  MARU_CONTROLLER_BUTTON_DPAD_DOWN = 13,
  MARU_CONTROLLER_BUTTON_DPAD_LEFT = 14,
  MARU_CONTROLLER_BUTTON_STANDARD_COUNT = 15
} MARU_ControllerButton;

typedef enum MARU_ControllerHaptic {
  MARU_CONTROLLER_HAPTIC_LOW_FREQ = 0,
  MARU_CONTROLLER_HAPTIC_HIGH_FREQ = 1,
  MARU_CONTROLLER_HAPTIC_STANDARD_COUNT = 2
} MARU_ControllerHaptic;

typedef struct MARU_ControllerList {
  /*
   * Borrowed snapshot whose storage may be reused by a later
   * maru_getControllers() call or by a later pump cycle. Retain handles you
   * need to keep.
   */
  MARU_Controller* const* controllers;
  uint32_t count;
} MARU_ControllerList;

static inline void* maru_getControllerUserdata(const MARU_Controller* controller);
static inline void maru_setControllerUserdata(MARU_Controller* controller, void* userdata);
static inline MARU_Context* maru_getControllerContext(const MARU_Controller* controller);
static inline bool maru_isControllerLost(const MARU_Controller* controller);
static inline const MARU_ControllerMetrics* maru_getControllerMetrics(
    const MARU_Controller* controller);
static inline const char* maru_getControllerName(const MARU_Controller* controller);
static inline uint16_t maru_getControllerVendorId(const MARU_Controller* controller);
static inline uint16_t maru_getControllerProductId(const MARU_Controller* controller);
static inline uint16_t maru_getControllerVersion(const MARU_Controller* controller);
static inline const uint8_t* maru_getControllerGUID(const MARU_Controller* controller);
static inline uint32_t maru_getControllerAnalogCount(const MARU_Controller* controller);
static inline const MARU_ChannelInfo* maru_getControllerAnalogChannelInfo(
    const MARU_Controller* controller);
static inline const MARU_AnalogInputState* maru_getControllerAnalogStates(
    const MARU_Controller* controller);
static inline uint32_t maru_getControllerButtonCount(const MARU_Controller* controller);
static inline const MARU_ChannelInfo* maru_getControllerButtonChannelInfo(
    const MARU_Controller* controller);
static inline const MARU_ButtonState8* maru_getControllerButtonStates(
    const MARU_Controller* controller);
static inline bool maru_isControllerButtonPressed(const MARU_Controller* controller,
                                                  uint32_t button_id);
static inline uint32_t maru_getControllerHapticCount(const MARU_Controller* controller);
static inline const MARU_ChannelInfo* maru_getControllerHapticChannelInfo(
    const MARU_Controller* controller);

MARU_API MARU_Status maru_getControllers(MARU_Context* context, MARU_ControllerList* out_list);
MARU_API void maru_retainController(MARU_Controller* controller);
MARU_API void maru_releaseController(MARU_Controller* controller);
MARU_API MARU_Status maru_resetControllerMetrics(MARU_Controller* controller);
MARU_API MARU_Status maru_getControllerInfo(const MARU_Controller* controller,
                                            MARU_ControllerInfo* out_info);
MARU_API MARU_Status maru_setControllerHapticLevels(MARU_Controller* controller,
                                                    uint32_t first_haptic,
                                                    uint32_t count,
                                                    const MARU_Scalar* intensities);

/* ----- Data exchange ----- */

/*
 * `mime_types` strings are copied before maru_announceData() returns
 * successfully.
 */
MARU_API MARU_Status maru_announceData(MARU_Window* window,
                                       MARU_DataExchangeTarget target,
                                       const char** mime_types,
                                       uint32_t count,
                                       MARU_DropActionMask allowed_actions);
/*
 * Fulfills a callback-scoped MARU_EVENT_DATA_REQUESTED request.
 *
 * If `flags` contains MARU_DATA_PROVIDE_FLAG_ZERO_COPY, `data` must remain
 * readable until the matching MARU_EVENT_DATA_CONSUMED callback fires or the
 * owning context is destroyed. Otherwise, the payload is copied before
 * maru_provideData() returns successfully.
 */
MARU_API MARU_Status maru_provideData(MARU_DataRequest* request,
                                      const void* data,
                                      size_t size,
                                      MARU_DataProvideFlags flags);
/* `mime_type` is copied before maru_requestData() returns successfully. */
MARU_API MARU_Status maru_requestData(MARU_Window* window,
                                      MARU_DataExchangeTarget target,
                                      const char* mime_type,
                                      void* userdata);
/*
 * Returns a borrowed snapshot of the currently available MIME types for the
 * requested target. The snapshot is invalidated by the next
 * maru_getAvailableMIMETypes() call on the same context/target, and may also
 * be replaced by a later pump cycle that changes the underlying offer.
 */
MARU_API MARU_Status maru_getAvailableMIMETypes(MARU_Window* window,
                                                MARU_DataExchangeTarget target,
                                                MARU_MIMETypeList* out_list);

static inline void maru_setDropSessionAction(MARU_DropSession* session, MARU_DropAction action);
static inline MARU_DropAction maru_getDropSessionAction(const MARU_DropSession* session);
static inline void maru_setDropSessionUserdata(MARU_DropSession* session, void* userdata);
static inline void* maru_getDropSessionUserdata(const MARU_DropSession* session);

/* ----- Vulkan ----- */

typedef void (*MARU_VulkanVoidFunction)(void);
typedef struct VkInstance_T* VkInstance;
typedef struct VkSurfaceKHR_T* VkSurfaceKHR;

typedef MARU_VulkanVoidFunction (*MARU_VkGetInstanceProcAddrFunc)(VkInstance instance,
                                                                  const char* pName);

typedef struct MARU_VkExtensionList {
  const char* const* names;
  uint32_t count;
} MARU_VkExtensionList;

/*
 * Returns a borrowed extension-name array. Backends may point this at static
 * storage; treat it as read-only and valid for at least the lifetime of the
 * context.
 */
MARU_API MARU_Status maru_getVkExtensions(const MARU_Context* context,
                                          MARU_VkExtensionList* out_list);
/* Requires a ready, non-lost window. */
MARU_API MARU_Status maru_createVkSurface(MARU_Window* window,
                                          VkInstance instance,
                                          MARU_VkGetInstanceProcAddrFunc vk_loader,
                                          VkSurfaceKHR* out_surface);

/* ----- Event queue helper ----- */

/*
 * Events excluded from this mask carry transient handles or borrowed pointer
 * payloads that are not safe to snapshot into a MARU_Queue without additional
 * copying or retention by the application.
 */
#define MARU_QUEUE_SAFE_EVENT_MASK                                                              \
  (MARU_EVENT_MASK(MARU_EVENT_CLOSE_REQUESTED) | MARU_EVENT_MASK(MARU_EVENT_WINDOW_RESIZED) |   \
   MARU_EVENT_MASK(MARU_EVENT_KEY_CHANGED) | MARU_EVENT_MASK(MARU_EVENT_WINDOW_READY) |         \
   MARU_EVENT_MASK(MARU_EVENT_MOUSE_MOVED) | MARU_EVENT_MASK(MARU_EVENT_MOUSE_BUTTON_CHANGED) | \
   MARU_EVENT_MASK(MARU_EVENT_MOUSE_SCROLLED) | MARU_EVENT_MASK(MARU_EVENT_IDLE_CHANGED) |      \
   MARU_EVENT_MASK(MARU_EVENT_WINDOW_FRAME) | MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_STARTED) |   \
   MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_ENDED) | MARU_EVENT_MASK(MARU_EVENT_USER_0) |           \
   MARU_EVENT_MASK(MARU_EVENT_USER_1) | MARU_EVENT_MASK(MARU_EVENT_USER_2) |                    \
   MARU_EVENT_MASK(MARU_EVENT_USER_3) | MARU_EVENT_MASK(MARU_EVENT_USER_4) |                    \
   MARU_EVENT_MASK(MARU_EVENT_USER_5) | MARU_EVENT_MASK(MARU_EVENT_USER_6) |                    \
   MARU_EVENT_MASK(MARU_EVENT_USER_7) | MARU_EVENT_MASK(MARU_EVENT_USER_8) |                    \
   MARU_EVENT_MASK(MARU_EVENT_USER_9) | MARU_EVENT_MASK(MARU_EVENT_USER_10) |                   \
   MARU_EVENT_MASK(MARU_EVENT_USER_11) | MARU_EVENT_MASK(MARU_EVENT_USER_12) |                  \
   MARU_EVENT_MASK(MARU_EVENT_USER_13) | MARU_EVENT_MASK(MARU_EVENT_USER_14) |                  \
   MARU_EVENT_MASK(MARU_EVENT_USER_15))

static inline bool maru_isQueueSafeEventId(MARU_EventId type) {
  return maru_eventMaskHas(MARU_QUEUE_SAFE_EVENT_MASK, type);
}

typedef struct MARU_QueueMetrics {
  uint32_t peak_active_count;
  uint32_t overflow_drop_count;
  uint32_t overflow_compact_count;
  uint32_t overflow_events_compacted;
  uint32_t overflow_drop_count_by_event[MARU_EVENT_USER_15 + 1];
} MARU_QueueMetrics;

MARU_API MARU_Status maru_createQueue(MARU_Context* ctx, uint32_t capacity, MARU_Queue** out_queue);
MARU_API void maru_destroyQueue(MARU_Queue* queue);
MARU_API MARU_Status maru_pushQueue(MARU_Queue* queue,
                                    MARU_EventId type,
                                    MARU_Window* window,
                                    const MARU_Event* event);
MARU_API MARU_Status maru_commitQueue(MARU_Queue* queue);
MARU_API void maru_scanQueue(MARU_Queue* queue,
                             MARU_EventMask mask,
                             MARU_EventCallback callback,
                             void* userdata);
MARU_API void maru_setQueueCoalesceMask(MARU_Queue* queue, MARU_EventMask mask);
MARU_API void maru_getQueueMetrics(const MARU_Queue* queue, MARU_QueueMetrics* out_metrics);
MARU_API void maru_resetQueueMetrics(MARU_Queue* queue);

/* ----- Convenience helpers ----- */

static inline MARU_Status maru_setContextInhibitsSystemIdle(MARU_Context* context, bool enabled);
static inline MARU_Status maru_setWindowTitle(MARU_Window* window, const char* title);
static inline MARU_Status maru_setWindowDipSize(MARU_Window* window, MARU_Vec2Dip size);
static inline MARU_Status maru_setWindowDipPosition(MARU_Window* window, MARU_Vec2Dip position);
static inline MARU_Status maru_setWindowFullscreen(MARU_Window* window, bool enabled);
static inline MARU_Status maru_setWindowMaximized(MARU_Window* window, bool enabled);
static inline MARU_Status maru_setWindowCursorMode(MARU_Window* window, MARU_CursorMode mode);
static inline MARU_Status maru_setWindowCursor(MARU_Window* window, MARU_Cursor* cursor);
static inline MARU_Status maru_setWindowMonitor(MARU_Window* window, MARU_Monitor* monitor);
static inline MARU_Status maru_setWindowMinDipSize(MARU_Window* window, MARU_Vec2Dip min_dip_size);
static inline MARU_Status maru_setWindowMaxDipSize(MARU_Window* window, MARU_Vec2Dip max_dip_size);
static inline MARU_Status maru_setWindowAspectRatio(MARU_Window* window,
                                                    MARU_Fraction aspect_ratio);
static inline MARU_Status maru_setWindowResizable(MARU_Window* window, bool enabled);
static inline MARU_Status maru_setWindowTextInputType(MARU_Window* window, MARU_TextInputType type);
static inline MARU_Status maru_setWindowTextInputRect(MARU_Window* window, MARU_RectDip rect);
static inline MARU_Status maru_setWindowAcceptDrop(MARU_Window* window, bool enabled);
static inline MARU_Status maru_setWindowVisible(MARU_Window* window, bool visible);
static inline MARU_Status maru_setWindowMinimized(MARU_Window* window, bool minimized);
static inline MARU_Status maru_setWindowIcon(MARU_Window* window, MARU_Image* icon);
static inline MARU_Status maru_requestText(MARU_Window* window,
                                           MARU_DataExchangeTarget target,
                                           void* user_tag);
static inline MARU_Status maru_configureWindowSimpleTextInput(MARU_Window* window,
                                                              MARU_TextInputType type);
static inline bool maru_applyTextEditCommitUtf8(char* buffer,
                                                uint32_t capacity_bytes,
                                                uint32_t* inout_length,
                                                uint32_t* inout_cursor_byte,
                                                const MARU_TextEditCommittedEvent* commit);
static inline const MARU_UserEventMetrics* maru_getContextEventMetrics(const MARU_Context* context);
static inline const char* maru_getDiagnosticString(MARU_Diagnostic diagnostic);

/* ----- Inline accessors and helper implementations ----- */

#include "maru/details/maru_details.h"

#ifdef __cplusplus
}
#endif

#endif
