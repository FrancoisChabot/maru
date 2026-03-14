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
 * 3. Direct inline state accessors, polling getters, native-handle getters,
 *    and userdata accessors (`maru_get*Userdata` / `maru_set*Userdata`) can
 *    be called from any thread with external synchronization to guarantee
 *    visibility with the owner thread.
 * 4. maru_postEvent(), maru_wakeContext(), maru_retain*(), maru_release*(),
 *    and maru_getVersion() are globally thread-safe and can be called from
 *    any thread without external synchronization.
 * 5. Backends may use internal helper threads for blocking OS work. These
 *    helpers never invoke your MARU_EventCallback directly; event callbacks are
 *    still only dispatched inline from maru_pumpEvents() on the owner thread.
 * 6. Unless documented otherwise, APIs not covered by the two rules above are
 *    owner-thread APIs.
 *
 * Ownership and lifetime model
 *
 * 1. Handle arguments are borrowed references. MARU does not take ownership of
 *    passed-in handles unless an API explicitly says so.
 * 2. Text and pixel payloads that MARU persists after a successful call are
 *    copied before that call returns unless an API explicitly documents
 *    zero-copy retention semantics.
 * 3. Lists, strings, and buffers returned by getters or exposed through event
 *    payloads are borrowed views. Copy what you need if it must outlive the
 *    documented lifetime.
 *
 * Validation note
 *
 * When MARU_VALIDATE_API_CALLS is enabled, invalid API usage is a contract
 * violation and will abort().
 *
 * C language mode note:
 *
 * The public C headers use designated initializers and anonymous unions. In C
 * builds, use C11 or newer, or an older dialect with equivalent compiler
 * extensions for those features.
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
typedef uint64_t MARU_WindowId;

#define MARU_BIT(n) ((uint64_t)1 << (n))
#define MARU_BIT32(n) ((uint32_t)1 << (n))
#define MARU_WINDOW_ID_NONE ((MARU_WindowId)0)

/**
 * Note: MARU_Status is not meant to tell you what went wrong, but rather what you can do about it.
 * It's a control flow mechanism, not a debugging tool. Use a diagnostic callback to get root cause
 * information.
 *
 * Invoking an api method on a lost context is **NOT** an API violation, but is guaranteed to be a
 * no-op. You do not need to explictly check for MARU_CONTEXT_LOST everywhere. Doing it on the
 * result of maru_pumpEvents() is often plenty.
 *
 * Handles owned by a lost context should be treated as inert. Status-returning
 * APIs on those child handles will typically short-circuit with
 * MARU_CONTEXT_LOST.
 *
 * This includes child-object destroy functions such as maru_destroyWindow(),
 * maru_destroyImage(), and maru_destroyCursor(): they are still backend-facing
 * teardown requests, so they participate in normal CONTEXT_LOST propagation.
 * By contrast, maru_destroyContext() is terminal local teardown and always
 * proceeds. Once a context is lost, destroy the context rather than trying to
 * tear down attached child objects one by one.
 *
 * Validation/early-out precedence for status-returning APIs is:
 * 1. Argument-related API violations are diagnosed first.
 * 2. If the arguments are valid but the owning context is lost, the API
 *    returns MARU_CONTEXT_LOST.
 * 3. Runtime-state preconditions that depend on a live backend (for example,
 *    requiring a ready window) are only meaningful after the context-loss
 *    check above.
 */
typedef enum MARU_Status {
  MARU_SUCCESS = 0,             // The operation succeeded
  MARU_FAILURE = 1,             // The operation failed, but the backend is still healthy
  MARU_CONTEXT_LOST = 2,  // The context is dead and now inert. You will have to rebuild it
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

#define MARU_GUID_SIZE 16u

typedef struct MARU_Guid {
  uint8_t bytes[MARU_GUID_SIZE];
} MARU_Guid;

typedef void* (*MARU_AllocationFunction)(size_t size, void* userdata);
typedef void* (*MARU_ReallocationFunction)(void* ptr, size_t new_size, void* userdata);
typedef void (*MARU_FreeFunction)(void* ptr, void* userdata);

typedef struct MARU_Allocator {
  MARU_AllocationFunction alloc_cb;
  MARU_ReallocationFunction realloc_cb;
  MARU_FreeFunction free_cb;
  void* userdata;
} MARU_Allocator;

typedef float MARU_Scalar;

typedef struct MARU_ChannelInfo {
  /* Human-readable channel label. Borrowed storage owned by the source handle. */
  const char* name;
  /* Backend/native code when one exists; UINT32_MAX if there is no stable native code. */
  uint32_t native_code;
  uint32_t flags;
  /*
   * Inclusive public value range for this channel.
   *
   * For button channels, Maru uses 0..1 to mean released/pressed semantics.
   * For scalar channels such as controller analogs and haptics, Maru guarantees
   * published values and accepted writes stay within this range.
   */
  MARU_Scalar min_value;
  MARU_Scalar max_value;
} MARU_ChannelInfo;

#define MARU_CHANNEL_FLAG_IS_DEFAULT MARU_BIT32(0)

MARU_API MARU_Version maru_getVersion(void);

/* ----- Geometry ----- */

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
  MARU_Vec2Dip position;
  MARU_Vec2Dip size;
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
  MARU_Vec2Dip dip_viewport_size;
  MARU_Vec2Px px_size;
  MARU_Scalar scale;
  MARU_BufferTransform buffer_transform;
} MARU_WindowGeometry;

/* ----- Tuning ----- */

typedef enum MARU_WaylandDecorationStrategy {
  MARU_WAYLAND_DECORATION_STRATEGY_AUTO = 0,
  MARU_WAYLAND_DECORATION_STRATEGY_SSD = 1,
  MARU_WAYLAND_DECORATION_STRATEGY_CSD = 2,
  MARU_WAYLAND_DECORATION_STRATEGY_NONE = 3,
} MARU_WaylandDecorationStrategy;

typedef struct MARU_ContextTuning {
  /*
   * Capacity of the application-posted user-event queue used by
   * maru_postEvent().
   *
   * A value of 0 disables application-posted user events for the context.
   * Backend/internal deferred events are unaffected.
   * Non-zero values must be powers of two.
   */
  uint32_t user_event_queue_size;

  struct {
    /*
     * Selects which Wayland decoration mechanism Maru should use for windows
     * that opt into decorations.
     */
    MARU_WaylandDecorationStrategy decoration_strategy;
  } wayland;

  struct {
    /*
     * X11-only timeout budget, in milliseconds, for synchronous selection /
     * clipboard round-trips that may be needed to complete a data-exchange
     * request during a pump.
     *
     * `0` means "do not wait": operations that would need to block for a
     * SelectionNotify/selection-owner reply fail immediately instead.
     *
     * This tuning does not affect the lifetime of clipboard ownership after
     * maru_announceClipboardData(); it only bounds how long Maru will block on
     * these X11 selection handshakes when it has to wait for another party.
     */
    uint32_t selection_query_timeout_ms;
  } x11;
} MARU_ContextTuning;

#define MARU_CONTEXT_TUNING_DEFAULT                                            \
  {                                                                            \
      .user_event_queue_size = 256,                                            \
      .wayland = {.decoration_strategy = MARU_WAYLAND_DECORATION_STRATEGY_AUTO}, \
      .x11 = {.selection_query_timeout_ms = 50},                                \
  }

/* ----- Public handles ----- */

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;
typedef struct MARU_Monitor MARU_Monitor;
typedef struct MARU_Controller MARU_Controller;
typedef struct MARU_Cursor MARU_Cursor;
typedef struct MARU_Image MARU_Image;
typedef struct MARU_DropSession MARU_DropSession;
typedef struct MARU_DataRequest MARU_DataRequest;

/* ----- Diagnostics ----- */

typedef enum MARU_Diagnostic {
  MARU_DIAGNOSTIC_NONE = 0,
  MARU_DIAGNOSTIC_INFO = 1,
  MARU_DIAGNOSTIC_OUT_OF_MEMORY = 2,
  MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE = 3,
  MARU_DIAGNOSTIC_DYNAMIC_LIB_FAILURE = 4,
  MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED = 5,
  MARU_DIAGNOSTIC_BACKEND_FAILURE = 6,
  MARU_DIAGNOSTIC_BACKEND_UNAVAILABLE = 7,
  MARU_DIAGNOSTIC_VULKAN_FAILURE = 8,
  MARU_DIAGNOSTIC_UNKNOWN = 9,
  MARU_DIAGNOSTIC_INVALID_ARGUMENT = 10,
  MARU_DIAGNOSTIC_PRECONDITION_FAILURE = 11,
  MARU_DIAGNOSTIC_INTERNAL = 12,
} MARU_Diagnostic;

typedef enum MARU_DiagnosticSubjectKind {
  MARU_DIAGNOSTIC_SUBJECT_NONE = 0,
  MARU_DIAGNOSTIC_SUBJECT_WINDOW = 1,
  MARU_DIAGNOSTIC_SUBJECT_MONITOR = 2,
  MARU_DIAGNOSTIC_SUBJECT_CONTROLLER = 3,
  MARU_DIAGNOSTIC_SUBJECT_CURSOR = 4,
  MARU_DIAGNOSTIC_SUBJECT_IMAGE = 5,
  MARU_DIAGNOSTIC_SUBJECT_DATA_REQUEST = 6,
} MARU_DiagnosticSubjectKind;

typedef union MARU_DiagnosticSubject {
  const MARU_Window* window;
  const MARU_Monitor* monitor;
  const MARU_Controller* controller;
  const MARU_Cursor* cursor;
  const MARU_Image* image;
  const MARU_DataRequest* data_request;
} MARU_DiagnosticSubject;

typedef struct MARU_DiagnosticInfo {
  MARU_Diagnostic diagnostic;
  /* Borrowed diagnostic string valid only for the duration of the callback. */
  const char* message;
  const MARU_Context* context;
  MARU_DiagnosticSubjectKind subject_kind;
  MARU_DiagnosticSubject subject;
} MARU_DiagnosticInfo;

typedef void (*MARU_DiagnosticCallback)(const MARU_DiagnosticInfo* info, void* userdata);

/* ----- Input model ----- */

typedef uint64_t MARU_ModifierFlags;

typedef enum MARU_ButtonState {
  MARU_BUTTON_STATE_RELEASED = 0,
  MARU_BUTTON_STATE_PRESSED = 1,
} MARU_ButtonState;

/*
* This exists so that button state array can be better packed
*/ 
typedef uint8_t MARU_ButtonState8;

typedef enum MARU_Key {
  MARU_KEY_UNKNOWN = 0,
  MARU_KEY_SPACE = 1,
  MARU_KEY_APOSTROPHE = 2,
  MARU_KEY_COMMA = 3,
  MARU_KEY_MINUS = 4,
  MARU_KEY_PERIOD = 5,
  MARU_KEY_SLASH = 6,
  MARU_KEY_0 = 7,
  MARU_KEY_1 = 8,
  MARU_KEY_2 = 9,
  MARU_KEY_3 = 10,
  MARU_KEY_4 = 11,
  MARU_KEY_5 = 12,
  MARU_KEY_6 = 13,
  MARU_KEY_7 = 14,
  MARU_KEY_8 = 15,
  MARU_KEY_9 = 16,
  MARU_KEY_SEMICOLON = 17,
  MARU_KEY_EQUAL = 18,
  MARU_KEY_A = 19,
  MARU_KEY_B = 20,
  MARU_KEY_C = 21,
  MARU_KEY_D = 22,
  MARU_KEY_E = 23,
  MARU_KEY_F = 24,
  MARU_KEY_G = 25,
  MARU_KEY_H = 26,
  MARU_KEY_I = 27,
  MARU_KEY_J = 28,
  MARU_KEY_K = 29,
  MARU_KEY_L = 30,
  MARU_KEY_M = 31,
  MARU_KEY_N = 32,
  MARU_KEY_O = 33,
  MARU_KEY_P = 34,
  MARU_KEY_Q = 35,
  MARU_KEY_R = 36,
  MARU_KEY_S = 37,
  MARU_KEY_T = 38,
  MARU_KEY_U = 39,
  MARU_KEY_V = 40,
  MARU_KEY_W = 41,
  MARU_KEY_X = 42,
  MARU_KEY_Y = 43,
  MARU_KEY_Z = 44,
  MARU_KEY_LEFT_BRACKET = 45,
  MARU_KEY_BACKSLASH = 46,
  MARU_KEY_RIGHT_BRACKET = 47,
  MARU_KEY_GRAVE_ACCENT = 48,
  MARU_KEY_ESCAPE = 49,
  MARU_KEY_ENTER = 50,
  MARU_KEY_TAB = 51,
  MARU_KEY_BACKSPACE = 52,
  MARU_KEY_INSERT = 53,
  MARU_KEY_DELETE = 54,
  MARU_KEY_RIGHT = 55,
  MARU_KEY_LEFT = 56,
  MARU_KEY_DOWN = 57,
  MARU_KEY_UP = 58,
  MARU_KEY_PAGE_UP = 59,
  MARU_KEY_PAGE_DOWN = 60,
  MARU_KEY_HOME = 61,
  MARU_KEY_END = 62,
  MARU_KEY_CAPS_LOCK = 63,
  MARU_KEY_SCROLL_LOCK = 64,
  MARU_KEY_NUM_LOCK = 65,
  MARU_KEY_PRINT_SCREEN = 66,
  MARU_KEY_PAUSE = 67,
  MARU_KEY_F1 = 68,
  MARU_KEY_F2 = 69,
  MARU_KEY_F3 = 70,
  MARU_KEY_F4 = 71,
  MARU_KEY_F5 = 72,
  MARU_KEY_F6 = 73,
  MARU_KEY_F7 = 74,
  MARU_KEY_F8 = 75,
  MARU_KEY_F9 = 76,
  MARU_KEY_F10 = 77,
  MARU_KEY_F11 = 78,
  MARU_KEY_F12 = 79,
  MARU_KEY_KP_0 = 80,
  MARU_KEY_KP_1 = 81,
  MARU_KEY_KP_2 = 82,
  MARU_KEY_KP_3 = 83,
  MARU_KEY_KP_4 = 84,
  MARU_KEY_KP_5 = 85,
  MARU_KEY_KP_6 = 86,
  MARU_KEY_KP_7 = 87,
  MARU_KEY_KP_8 = 88,
  MARU_KEY_KP_9 = 89,
  MARU_KEY_KP_DECIMAL = 90,
  MARU_KEY_KP_DIVIDE = 91,
  MARU_KEY_KP_MULTIPLY = 92,
  MARU_KEY_KP_SUBTRACT = 93,
  MARU_KEY_KP_ADD = 94,
  MARU_KEY_KP_ENTER = 95,
  MARU_KEY_KP_EQUAL = 96,
  MARU_KEY_LEFT_SHIFT = 97,
  MARU_KEY_LEFT_CONTROL = 98,
  MARU_KEY_LEFT_ALT = 99,
  MARU_KEY_LEFT_META = 100,
  MARU_KEY_RIGHT_SHIFT = 101,
  MARU_KEY_RIGHT_CONTROL = 102,
  MARU_KEY_RIGHT_ALT = 103,
  MARU_KEY_RIGHT_META = 104,
  MARU_KEY_MENU = 105,
  MARU_KEY_COUNT = 106
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
/*
 * Default mouse buttons are always present in the first
 * `MARU_MOUSE_DEFAULT_COUNT` mouse button channels, in enum order. Extra
 * backend-specific mouse buttons, if any, always follow after that range.
 */

typedef struct MARU_AnalogInputState {
  MARU_Scalar value;
} MARU_AnalogInputState;

/*
 * Keyboard polling is indexed directly by `MARU_Key`.
 *
 * The returned state array always contains exactly `MARU_KEY_COUNT` entries in
 * enum order, including `MARU_KEY_UNKNOWN` at index 0.
 */
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
  MARU_EVENT_DATA_RELEASED = 22,
  MARU_EVENT_DRAG_FINISHED = 23,
  MARU_EVENT_CONTROLLER_CHANGED = 24,
  MARU_EVENT_CONTROLLER_BUTTON_CHANGED = 25,
  MARU_EVENT_TEXT_EDIT_NAVIGATION = 26,

  /* Ids 27 to 47 are reserved for future additions.
  * 
  * User event bits are permanently pinned to the end of the range.
  */

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
  /* There can be no EventID above 63, since they are used as bit indices. */
} MARU_EventId;

/*
 * Constant-expression primitive for known-valid event ids.
 *
 * For arbitrary runtime values, use maru_getEventMask() instead.
 */
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
#define MARU_MASK_DATA_RELEASED MARU_EVENT_MASK(MARU_EVENT_DATA_RELEASED)
#define MARU_MASK_DRAG_FINISHED MARU_EVENT_MASK(MARU_EVENT_DRAG_FINISHED)
#define MARU_MASK_CONTROLLER_CHANGED MARU_EVENT_MASK(MARU_EVENT_CONTROLLER_CHANGED)
#define MARU_MASK_CONTROLLER_BUTTON_CHANGED MARU_EVENT_MASK(MARU_EVENT_CONTROLLER_BUTTON_CHANGED)
#define MARU_MASK_TEXT_EDIT_NAVIGATION MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_NAVIGATION)
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
#define MARU_ALL_EVENTS                                                                            \
  (MARU_MASK_CLOSE_REQUESTED | MARU_MASK_WINDOW_RESIZED | MARU_MASK_KEY_CHANGED |                  \
   MARU_MASK_WINDOW_READY | MARU_MASK_MOUSE_MOVED | MARU_MASK_MOUSE_BUTTON_CHANGED |               \
   MARU_MASK_MOUSE_SCROLLED | MARU_MASK_IDLE_CHANGED | MARU_MASK_MONITOR_CHANGED |                 \
   MARU_MASK_MONITOR_MODE_CHANGED | MARU_MASK_WINDOW_FRAME | MARU_MASK_WINDOW_STATE_CHANGED |      \
   MARU_MASK_TEXT_EDIT_STARTED | MARU_MASK_TEXT_EDIT_UPDATED | MARU_MASK_TEXT_EDIT_COMMITTED |     \
   MARU_MASK_TEXT_EDIT_ENDED | MARU_MASK_DROP_ENTERED | MARU_MASK_DROP_HOVERED |                   \
   MARU_MASK_DROP_EXITED | MARU_MASK_DROP_DROPPED | MARU_MASK_DATA_RECEIVED |                      \
   MARU_MASK_DATA_REQUESTED | MARU_MASK_DATA_RELEASED | MARU_MASK_DRAG_FINISHED |                  \
   MARU_MASK_CONTROLLER_CHANGED | MARU_MASK_CONTROLLER_BUTTON_CHANGED |                             \
   MARU_MASK_TEXT_EDIT_NAVIGATION | MARU_MASK_USER_0 | MARU_MASK_USER_1 | MARU_MASK_USER_2 |      \
   MARU_MASK_USER_3 | MARU_MASK_USER_4 | MARU_MASK_USER_5 | MARU_MASK_USER_6 |                     \
   MARU_MASK_USER_7 | MARU_MASK_USER_8 | MARU_MASK_USER_9 | MARU_MASK_USER_10 |                    \
   MARU_MASK_USER_11 | MARU_MASK_USER_12 | MARU_MASK_USER_13 | MARU_MASK_USER_14 |                 \
   MARU_MASK_USER_15)

static inline bool maru_isUserEventId(MARU_EventId id) {
  return id >= MARU_EVENT_USER_0 && id <= MARU_EVENT_USER_15;
}

static inline MARU_EventMask maru_getEventMask(MARU_EventId id) {
  const uint32_t raw_id = (uint32_t)id;
  return (raw_id < 64u) ? MARU_EVENT_MASK(raw_id) : 0u;
}

static inline bool maru_isKnownEventId(MARU_EventId id) {
  return (maru_getEventMask(id) & (MARU_ALL_EVENTS)) != 0;
}

static inline bool maru_eventMaskHas(MARU_EventMask mask, MARU_EventId id) {
  return (mask & maru_getEventMask(id)) != 0;
}

static inline MARU_EventMask maru_eventMaskAdd(MARU_EventMask mask, MARU_EventId id) {
  return mask | maru_getEventMask(id);
}

static inline MARU_EventMask maru_eventMaskRemove(MARU_EventMask mask, MARU_EventId id) {
  return mask & ~maru_getEventMask(id);
}

typedef struct MARU_WindowReadyEvent {
  MARU_WindowGeometry geometry;
} MARU_WindowReadyEvent;

typedef struct MARU_CloseRequestedEvent {
  char _unused;
} MARU_CloseRequestedEvent;

typedef struct MARU_WindowFrameEvent {
  /*
   * Backend-provided millisecond frame timestamp.
   *
   * This value is 32-bit and may wrap after roughly 49.7 days of uptime.
   * Callers that care about long-running timeline arithmetic must handle
   * wraparound themselves.
   */
  uint32_t timestamp_ms;
} MARU_WindowFrameEvent;

typedef struct MARU_WindowResizedEvent {
  MARU_WindowGeometry geometry;
} MARU_WindowResizedEvent;

typedef uint32_t MARU_WindowStateChangedFlags;
#define MARU_WINDOW_STATE_CHANGED_VISIBLE MARU_BIT32(0)
#define MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE MARU_BIT32(1)
#define MARU_WINDOW_STATE_CHANGED_MINIMIZED MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE
#define MARU_WINDOW_STATE_CHANGED_MAXIMIZED MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE
#define MARU_WINDOW_STATE_CHANGED_FULLSCREEN MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE
#define MARU_WINDOW_STATE_CHANGED_FOCUSED MARU_BIT32(3)
#define MARU_WINDOW_STATE_CHANGED_ICON MARU_BIT32(4)
#define MARU_WINDOW_STATE_CHANGED_RESIZABLE MARU_BIT32(6)

typedef enum MARU_WindowPresentationState {
  MARU_WINDOW_PRESENTATION_NORMAL = 0,
  MARU_WINDOW_PRESENTATION_FULLSCREEN = 1,
  MARU_WINDOW_PRESENTATION_MAXIMIZED = 2,
  MARU_WINDOW_PRESENTATION_MINIMIZED = 3,
} MARU_WindowPresentationState;

typedef struct MARU_WindowStateChangedEvent {
  MARU_WindowStateChangedFlags changed_fields;
  MARU_WindowPresentationState presentation_state;
  bool visible;
  bool focused;
  bool resizable;
  /* Borrowed same-context handle to the window's current icon, if any. */
  const MARU_Image* icon;
} MARU_WindowStateChangedEvent;

typedef struct MARU_KeyChangedEvent {
  /* Maru-normalized logical key, not a backend-native raw key/scancode. */
  MARU_Key key;
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
  MARU_DROP_ACTION_COPY = MARU_BIT32(0),
  MARU_DROP_ACTION_MOVE = MARU_BIT32(1),
  MARU_DROP_ACTION_LINK = MARU_BIT32(2),
} MARU_DropAction;

typedef uint64_t MARU_DropActionMask;

typedef enum MARU_DataExchangeTarget {
  /* Public request/receive events report which data-exchange workflow they belong to. */
  MARU_DATA_EXCHANGE_TARGET_CLIPBOARD = 0,
  MARU_DATA_EXCHANGE_TARGET_DRAG_DROP = 1,
} MARU_DataExchangeTarget;

typedef struct MARU_StringList {
  const char* const* strings;
  uint32_t count;
} MARU_StringList;

typedef uint32_t MARU_DataProvideFlags;
#define MARU_DATA_PROVIDE_FLAG_NONE 0
#define MARU_DATA_PROVIDE_FLAG_ZERO_COPY MARU_BIT32(0)

typedef struct MARU_DropEvent {
  MARU_Vec2Dip dip_position;
  /*
   * Callback-scoped handle for the current logical drag session. Do not store
   * it beyond the callback that delivered this event.
   *
   * maru_setDropSessionUserdata() stores application state on the underlying
   * logical drag session so that later DnD callbacks in the same session can
   * recover it through their own callback-scoped handle.
   */
  MARU_DropSession* session;
  /*
   * Source-offered drag actions as currently known to Maru for this session.
   *
   * This may be 0 if the backend has not yet observed any actionable offer for
   * the current drag session. maru_setDropSessionAction() accepts only
   * MARU_DROP_ACTION_NONE or one of these single-bit actions.
   */
  MARU_DropActionMask available_actions;
  /* Borrowed callback-scoped pointers. Copy what you need before returning. */
  MARU_StringList paths;
  MARU_StringList available_types;
  MARU_ModifierFlags modifiers;
} MARU_DropEvent;

typedef struct MARU_DropExitedEvent {
  /*
   * Callback-scoped handle for the logical drag session that just ended for
   * this window. Do not store it beyond the callback that delivered this
   * event.
   */
  MARU_DropSession* session;
} MARU_DropExitedEvent;

typedef struct MARU_DataReceivedEvent {
  /*
   * Opaque request token round-tripped from the corresponding
   * maru_requestClipboardData() or maru_requestDropData() call.
   */
  void* userdata;
  MARU_Status status;
  /* Selects the matching request/provide workflow for this completed transfer. */
  MARU_DataExchangeTarget target;
  /* Borrowed callback-scoped pointers. Copy what you need before returning. */
  const char* mime_type;
  const void* data;
  size_t size;
} MARU_DataReceivedEvent;

typedef struct MARU_DataRequestEvent {
  /* Selects whether maru_provideClipboardData() or maru_provideDropData() is valid. */
  MARU_DataExchangeTarget target;
  /*
   * `request` and `mime_type` are only valid for the duration of the
   * MARU_EVENT_DATA_REQUESTED callback that delivered them.
   */
  const char* mime_type;
  MARU_DataRequest* request;
} MARU_DataRequestEvent;

typedef struct MARU_DataReleasedEvent {
  /*
   * Identifies whether the released payload belonged to clipboard or
   * drag/drop.
   */
  MARU_DataExchangeTarget target;
  /*
   * Borrowed callback-scoped pointers describing the payload Maru no longer
   * needs. Copy what you need before returning.
   */
  const char* mime_type;
  const void* data;
  size_t size;
  /*
   * Meaningful only for drag/drop release. When `target` is
   * MARU_DATA_EXCHANGE_TARGET_CLIPBOARD, ignore this field.
   */
  MARU_DropAction action;
} MARU_DataReleasedEvent;

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
  /* Byte offset into the associated UTF-8 string. */
  uint32_t start_byte;
  /* Byte length within the associated UTF-8 string. */
  uint32_t length_bytes;
} MARU_TextRangeUtf8;

typedef struct MARU_TextEditStartedEvent {
  uint64_t session_id;
} MARU_TextEditStartedEvent;

typedef struct MARU_TextEditUpdatedEvent {
  uint64_t session_id;
  /*
   * Borrowed callback-scoped UTF-8. Copy what you need before returning.
   *
   * `preedit_length_bytes` excludes the trailing NUL. `caret` and `selection`
   * are byte ranges into `preedit_utf8` and always fall on UTF-8 code point
   * boundaries.
   */
  const char* preedit_utf8;
  uint32_t preedit_length_bytes;
  MARU_TextRangeUtf8 caret;
  MARU_TextRangeUtf8 selection;
} MARU_TextEditUpdatedEvent;

typedef struct MARU_TextEditCommittedEvent {
  uint64_t session_id;
  /*
   * Byte counts relative to the current insertion cursor in the target UTF-8
   * buffer. These deletion spans always remove whole UTF-8 code points.
   */
  uint32_t delete_before_bytes;
  uint32_t delete_after_bytes;
  /*
   * Borrowed callback-scoped UTF-8. Copy what you need before returning.
   *
   * `committed_length_bytes` excludes the trailing NUL. When non-zero,
   * `committed_utf8` always contains valid UTF-8.
   */
  const char* committed_utf8;
  uint32_t committed_length_bytes;
} MARU_TextEditCommittedEvent;

typedef struct MARU_TextEditEndedEvent {
  uint64_t session_id;
  bool canceled;
} MARU_TextEditEndedEvent;

typedef enum MARU_TextEditNavigationCommand {
  MARU_TEXT_EDIT_NAVIGATE_LEFT = 0,
  MARU_TEXT_EDIT_NAVIGATE_RIGHT = 1,
  MARU_TEXT_EDIT_NAVIGATE_UP = 2,
  MARU_TEXT_EDIT_NAVIGATE_DOWN = 3,
  MARU_TEXT_EDIT_NAVIGATE_WORD_LEFT = 4,
  MARU_TEXT_EDIT_NAVIGATE_WORD_RIGHT = 5,
  MARU_TEXT_EDIT_NAVIGATE_LINE_START = 6,
  MARU_TEXT_EDIT_NAVIGATE_LINE_END = 7,
  MARU_TEXT_EDIT_NAVIGATE_DOCUMENT_START = 8,
  MARU_TEXT_EDIT_NAVIGATE_DOCUMENT_END = 9,
} MARU_TextEditNavigationCommand;

/*
 * Semantic caret-navigation command for active text editing.
 *
 * This exists because raw keyboard events intentionally do not auto-repeat, but
 * text boxes still need repeated caret movement and selection navigation while
 * an IME/text-input session is active. Unlike UTF-8 commit events, this does
 * not edit text directly; it reports navigation intent for the app's own text
 * model and layout.
 */
typedef struct MARU_TextEditNavigationEvent {
  uint64_t session_id;
  MARU_TextEditNavigationCommand command;
  bool extend_selection;
  bool is_repeat;
  MARU_ModifierFlags modifiers;
} MARU_TextEditNavigationEvent;

typedef struct MARU_UserDefinedEvent {
  union {
    void* userdata;
    /*
     * Raw byte payload with the same starting address as `userdata`.
     *
     * This union includes `long double` to guarantee at least 16-byte alignment
     * on most modern platforms. This allows users to safely cast the start of
     * the payload to any standard scalar or SIMD type that fits in the buffer
     * and requires 16-byte alignment or less.
     */
    char raw_payload[64];
    long double _align_ld;
    long long _align_ll;
  };
} MARU_UserDefinedEvent;

typedef struct MARU_Event {
  union {
    MARU_WindowReadyEvent window_ready;
    MARU_CloseRequestedEvent close_requested;
    MARU_WindowResizedEvent window_resized;
    MARU_WindowStateChangedEvent window_state_changed;
    MARU_KeyChangedEvent key_changed;
    MARU_MouseMovedEvent mouse_moved;
    MARU_MouseButtonChangedEvent mouse_button_changed;
    MARU_MouseScrolledEvent mouse_scrolled;
    MARU_IdleChangedEvent idle_changed;
    MARU_MonitorChangedEvent monitor_changed;
    MARU_MonitorModeChangedEvent monitor_mode_changed;
    MARU_DropEvent drop_entered;
    MARU_DropEvent drop_hovered;
    MARU_DropExitedEvent drop_exited;
    MARU_DropEvent drop_dropped;
    MARU_DataReceivedEvent data_received;
    MARU_DataRequestEvent data_requested;
    MARU_DataReleasedEvent data_released;
    MARU_DragFinishedEvent drag_finished;
    MARU_ControllerChangedEvent controller_changed;
    MARU_ControllerButtonChangedEvent controller_button_changed;
    MARU_WindowFrameEvent window_frame;
    MARU_TextEditStartedEvent text_edit_started;
    MARU_TextEditUpdatedEvent text_edit_updated;
    MARU_TextEditCommittedEvent text_edit_committed;
    MARU_TextEditEndedEvent text_edit_ended;
    MARU_TextEditNavigationEvent text_edit_navigation;
    MARU_UserDefinedEvent user;
  };
} MARU_Event;

/* 
 * MARU_Event is strictly capped at 64 bytes to ensure it fits within a single
 * standard CPU cache line. This minimizes cache pressure and latency during
 * high-frequency event dispatch (e.g., raw mouse motion).
 */
MARU_STATIC_ASSERT(sizeof(MARU_Event) <= 64, "MARU_Event ABI Violation: Size exceeds 64 bytes.");
MARU_STATIC_ASSERT(sizeof(MARU_UserDefinedEvent) == 64,
                   "MARU_UserDefinedEvent ABI Violation: Expected 64 bytes.");

/*
 * Direct pump callback. Matching events are delivered synchronously and inline
 * from maru_pumpEvents().
 *
 * `window` is NULL for context-scoped events such as monitor/controller
 * changes, clipboard data-exchange events, and user events posted via
 * maru_postEvent().
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

/*
 * Thread-safe user-event injection for a later pump cycle.
 *
 * This will also implicitly wake the context.
 *
 * `type` must be one of the user-defined event ids
 * (`MARU_EVENT_USER_0` .. `MARU_EVENT_USER_15`).
 *
 * Posted user events are always dispatched as context-scoped events, so the
 * callback receives `window == NULL`.
 *
 * Returns true if the event was successfully queued, false otherwise. This
 * returns false when application-posted user events are disabled for the
 * context (`user_event_queue_size == 0`). Backend/internal deferred events are
 * unaffected by this setting.
 */
MARU_API bool maru_postEvent(MARU_Context* context,
                             MARU_EventId type,
                             MARU_UserDefinedEvent evt);

/*
 * Thread-safe wakeup of a sleeping pump without having to post an event.
 *
 * Returns true if the context was successfully woken, false otherwise.
 */
MARU_API bool maru_wakeContext(MARU_Context* context);

/* ----- Contexts ----- */


#define MARU_NEVER UINT32_MAX
#define MARU_CONTEXT_ATTR_INHIBIT_IDLE MARU_BIT(0)
#define MARU_CONTEXT_ATTR_DIAGNOSTICS MARU_BIT(1)
#define MARU_CONTEXT_ATTR_IDLE_TIMEOUT MARU_BIT(2)
#define MARU_CONTEXT_ATTR_ALL                                       \
  (MARU_CONTEXT_ATTR_INHIBIT_IDLE | MARU_CONTEXT_ATTR_DIAGNOSTICS | \
   MARU_CONTEXT_ATTR_IDLE_TIMEOUT)

/* Userdata accessors follow the direct-inline-accessor threading rule. */
static inline void* maru_getContextUserdata(const MARU_Context* context);
static inline void maru_setContextUserdata(MARU_Context* context, void* userdata);
static inline bool maru_isContextLost(const MARU_Context* context);
static inline MARU_BackendType maru_getContextBackend(const MARU_Context* context);
static inline uint32_t maru_getMouseButtonCount(const MARU_Context* context);
/*
 * The first `MARU_MOUSE_DEFAULT_COUNT` entries are always the default mouse
 * buttons in `MARU_MouseDefaultButton` order.
 *
 * Inline polling helpers that take a channel/key index assume the index is in
 * range. With API validation enabled, out-of-range indices are diagnosed as an
 * API violation. Otherwise they are undefined behavior.
 */
static inline const MARU_ButtonState8* maru_getMouseButtonStates(
    const MARU_Context* context);
static inline const MARU_ChannelInfo* maru_getMouseButtonChannelInfo(
    const MARU_Context* context);
static inline bool maru_isMouseButtonPressed(const MARU_Context* context,
                                             uint32_t button_id);

typedef struct MARU_ContextAttributes {
  /*
   * When MARU_ENABLE_DIAGNOSTICS is disabled, registering this callback is
   * still valid but the callback is never invoked.
   */
  MARU_DiagnosticCallback diagnostic_cb;
  void* diagnostic_userdata;
  /*
   * Requests that Maru inhibit the OS/system idle mechanism while the context
   * is alive.
   *
   * This is advisory and backend-dependent. Backends that cannot honor the
   * request keep running normally and may report a diagnostic.
   */
  bool inhibit_idle;
  /*
   * Idle threshold, in milliseconds, used for `MARU_EVENT_IDLE_CHANGED`.
   *
   * `0` disables idle-change tracking for the context and no
   * `MARU_EVENT_IDLE_CHANGED` events will be emitted.
   *
   * When non-zero, the event reports this same configured threshold in
   * `event->idle_changed.timeout_ms`.
   */
  uint32_t idle_timeout_ms;
} MARU_ContextAttributes;

/*
 * Context creation contract:
 *
 * - `allocator` uses the default allocator when all callbacks are NULL.
 * - Custom allocators are all-or-none: if any callback is provided, `alloc_cb`,
 *   `realloc_cb`, and `free_cb` must all be non-null.
 * - `backend = MARU_BACKEND_UNKNOWN` lets Maru choose the native backend using
 *   the documented platform-default selection rules.
 * - `attributes` and `tuning` are copied by maru_createContext().
 * - `userdata` is stored directly on the created context and can later be
 *   accessed via maru_getContextUserdata() / maru_setContextUserdata().
 */
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
 * You do not need to destroy windows, cursors, images, or other context-owned
 * child objects one by one before destroying the context.
 *
 * This is unconditional local teardown, not a backend-facing request, so it
 * always returns void and proceeds regardless of whether the context is live or
 * already lost.
 */
MARU_API void maru_destroyContext(MARU_Context* context);
MARU_API MARU_Status maru_updateContext(MARU_Context* context,
                                        uint64_t field_mask,
                                        const MARU_ContextAttributes* attributes);

/* ----- Images ----- */

typedef struct MARU_ImageCreateInfo {
  /* Image dimensions in pixels. Both axes must be strictly positive. */
  MARU_Vec2Px px_size;
  /*
   * Copied into MARU-owned storage before maru_createImage() returns.
   *
   * Must be non-null and point to at least `px_size.y` rows of pixel data.
   */
  const uint32_t* pixels;
  /*
   * Number of bytes per row. If 0, assumes tightly packed (width * 4 bytes).
   * If nonzero, it must be at least `px_size.x * 4`.
   */
  uint32_t stride_bytes;
  void* userdata;
} MARU_ImageCreateInfo;

MARU_API MARU_Status maru_createImage(MARU_Context* context,
                                      const MARU_ImageCreateInfo* create_info,
                                      MARU_Image** out_image);
/*
 * Destroys an image and frees its resources.
 *
 * If the image is currently set as an icon for one or more windows, those
 * windows will automatically revert to their default icon.
 *
 * Unlike maru_destroyContext(), this is still a backend-facing teardown
 * request. If the owning context is already lost, it short-circuits with
 * MARU_CONTEXT_LOST; destroy the context itself for terminal cleanup.
 */
MARU_API MARU_Status maru_destroyImage(MARU_Image* image);
/* Userdata accessors follow the direct-inline-accessor threading rule. */
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

/* Userdata accessors follow the direct-inline-accessor threading rule. */
static inline void* maru_getCursorUserdata(const MARU_Cursor* cursor);
static inline void maru_setCursorUserdata(MARU_Cursor* cursor, void* userdata);
static inline bool maru_isCursorSystem(const MARU_Cursor* cursor);

typedef struct MARU_CursorFrame {
  /* Referenced image copied into cursor-owned storage during creation. */
  const MARU_Image* image;
  MARU_Vec2Px px_hot_spot;
  /*
   * Display duration for this frame in milliseconds when used in a multi-frame
   * custom cursor. A value of 0 is treated as 1 ms. Single-frame custom
   * cursors ignore this field.
   */
  uint32_t delay_ms;
} MARU_CursorFrame;

typedef struct MARU_CursorCreateInfo {
  MARU_CursorSource source;
  /*
   * For system cursors, creation fails if the backend cannot provide the
   * requested native/system cursor shape. Maru does not synthesize fallback
   * system cursors. Ignore `frames` and `frame_count` in this mode.
   */
  MARU_CursorShape system_shape;
  /*
   * For custom cursors, the referenced image pixels are copied into
   * cursor-owned frames during maru_createCursor(). Ignore `system_shape` in
   * this mode. `frame_count` must be non-zero and `frames` must point to that
   * many entries. `frame_count > 1` creates an animated cursor that advances
   * through the frames in order and loops.
   */
  const MARU_CursorFrame* frames;
  uint32_t frame_count;
  void* userdata;
} MARU_CursorCreateInfo;

MARU_API MARU_Status maru_createCursor(MARU_Context* context,
                                       const MARU_CursorCreateInfo* create_info,
                                       MARU_Cursor** out_cursor);

/*
 * Destroys a cursor and frees its resources.
 *
 * If the cursor is currently active on one or more windows, those windows
 * will automatically revert to the system default cursor.
 *
 * Unlike maru_destroyContext(), this is still a backend-facing teardown
 * request. If the owning context is already lost, it short-circuits with
 * MARU_CONTEXT_LOST; destroy the context itself for terminal cleanup.
 */
MARU_API MARU_Status maru_destroyCursor(MARU_Cursor* cursor);

/* ----- Monitors ----- */

typedef struct MARU_VideoMode {
  MARU_Vec2Px px_size;
  uint32_t refresh_rate_millihz;
} MARU_VideoMode;

typedef struct MARU_VideoModeList {
    /*
   * Borrowed monitor-owned storage. Valid until the monitor is lost or
   * destroyed, or until the backend refreshes that monitor's mode cache.
   * Reacquire after monitor topology/mode changes if you need a fresh view.
   */
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

/*
 * Lost-monitor contract:
 *
 * A lost monitor handle remains valid in memory until you release it.
 * Accessors and getters continue to operate on the last known snapshot and may
 * therefore return stale information after loss. Backend-facing mutators
 * become inert and return MARU_FAILURE once the monitor is lost.
 */
/* Userdata accessors follow the direct-inline-accessor threading rule. */
static inline void* maru_getMonitorUserdata(const MARU_Monitor* monitor);
static inline void maru_setMonitorUserdata(MARU_Monitor* monitor, void* userdata);
static inline const MARU_Context* maru_getMonitorContext(const MARU_Monitor* monitor);
static inline bool maru_isMonitorLost(const MARU_Monitor* monitor);
static inline const char* maru_getMonitorName(const MARU_Monitor* monitor);
static inline MARU_Vec2Mm maru_getMonitorPhysicalSize(const MARU_Monitor* monitor);
static inline MARU_VideoMode maru_getMonitorCurrentMode(const MARU_Monitor* monitor);
static inline MARU_Vec2Dip maru_getMonitorDipPosition(const MARU_Monitor* monitor);
static inline MARU_Vec2Dip maru_getMonitorDipSize(const MARU_Monitor* monitor);
static inline bool maru_isMonitorPrimary(const MARU_Monitor* monitor);
static inline MARU_Scalar maru_getMonitorScale(const MARU_Monitor* monitor);

MARU_API MARU_Status maru_getMonitors(const MARU_Context* context, MARU_MonitorList* out_list);
MARU_API void maru_retainMonitor(MARU_Monitor* monitor);
MARU_API void maru_releaseMonitor(MARU_Monitor* monitor);
/*
 * Returns a borrowed mode list owned by `monitor`; see MARU_VideoModeList for
 * lifetime details.
 */
MARU_API MARU_Status maru_getMonitorModes(const MARU_Monitor* monitor,
                                          MARU_VideoModeList* out_list);
MARU_API MARU_Status maru_setMonitorMode(MARU_Monitor* monitor, MARU_VideoMode mode);

/* ----- Windows ----- */

typedef enum MARU_CursorMode {
  MARU_CURSOR_NORMAL = 0,
  MARU_CURSOR_HIDDEN = 1,
  MARU_CURSOR_LOCKED = 2,
} MARU_CursorMode;

typedef enum MARU_ContentType {
  /*
   * Advisory window content classification.
   *
   * This is creation-time metadata only. Maru forwards it to platform
   * facilities when a backend exposes a native concept for content type; on
   * backends without such a facility it has no observable effect.
   *
   * It does not affect Maru's rendering, event delivery, timing, or input
   * semantics.
   */
  MARU_CONTENT_TYPE_NONE = 0,
  MARU_CONTENT_TYPE_PHOTO = 1,
  MARU_CONTENT_TYPE_VIDEO = 2,
  MARU_CONTENT_TYPE_GAME = 3,
} MARU_ContentType;

typedef enum MARU_TextInputType {
  MARU_TEXT_INPUT_TYPE_NONE = 0,
  MARU_TEXT_INPUT_TYPE_TEXT = 1,
  MARU_TEXT_INPUT_TYPE_PASSWORD = 2,
  MARU_TEXT_INPUT_TYPE_EMAIL = 3,
  MARU_TEXT_INPUT_TYPE_NUMERIC = 4,
} MARU_TextInputType;

/* Userdata accessors follow the direct-inline-accessor threading rule. */
static inline void* maru_getWindowUserdata(const MARU_Window* window);
static inline void maru_setWindowUserdata(MARU_Window* window, void* userdata);
static inline const MARU_Context* maru_getWindowContext(const MARU_Window* window);
static inline MARU_WindowId maru_getWindowId(const MARU_Window* window);
static inline const char* maru_getWindowTitle(const MARU_Window* window);
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
static inline MARU_WindowGeometry maru_getWindowGeometry(const MARU_Window* window);

#define MARU_WINDOW_ATTR_TITLE MARU_BIT(0)
#define MARU_WINDOW_ATTR_DIP_SIZE MARU_BIT(1)
#define MARU_WINDOW_ATTR_PRESENTATION_STATE MARU_BIT(2)
#define MARU_WINDOW_ATTR_CURSOR_MODE MARU_BIT(3)
#define MARU_WINDOW_ATTR_CURSOR MARU_BIT(4)
#define MARU_WINDOW_ATTR_MONITOR MARU_BIT(5)
#define MARU_WINDOW_ATTR_DIP_MIN_SIZE MARU_BIT(7)
#define MARU_WINDOW_ATTR_DIP_MAX_SIZE MARU_BIT(8)
#define MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE MARU_BIT(9)
#define MARU_WINDOW_ATTR_DIP_POSITION MARU_BIT(10)
#define MARU_WINDOW_ATTR_ASPECT_RATIO MARU_BIT(11)
#define MARU_WINDOW_ATTR_RESIZABLE MARU_BIT(12)
#define MARU_WINDOW_ATTR_ACCEPT_DROP MARU_BIT(13)
#define MARU_WINDOW_ATTR_TEXT_INPUT_TYPE MARU_BIT(14)
#define MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT MARU_BIT(15)
#define MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE MARU_BIT(16)
#define MARU_WINDOW_ATTR_SURROUNDING_TEXT MARU_BIT(17)
#define MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE MARU_BIT(18)
#define MARU_WINDOW_ATTR_VISIBLE MARU_BIT(19)
#define MARU_WINDOW_ATTR_ICON MARU_BIT(21)

#define MARU_WINDOW_ATTR_ALL                                                                    \
  (MARU_WINDOW_ATTR_TITLE | MARU_WINDOW_ATTR_DIP_SIZE | MARU_WINDOW_ATTR_PRESENTATION_STATE |     \
   MARU_WINDOW_ATTR_CURSOR_MODE | MARU_WINDOW_ATTR_CURSOR | MARU_WINDOW_ATTR_MONITOR |           \
   MARU_WINDOW_ATTR_DIP_MIN_SIZE | MARU_WINDOW_ATTR_DIP_MAX_SIZE | MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE | \
   MARU_WINDOW_ATTR_DIP_POSITION | MARU_WINDOW_ATTR_ASPECT_RATIO | MARU_WINDOW_ATTR_RESIZABLE |    \
   MARU_WINDOW_ATTR_ACCEPT_DROP | MARU_WINDOW_ATTR_TEXT_INPUT_TYPE | MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT | \
   MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE | MARU_WINDOW_ATTR_SURROUNDING_TEXT |                  \
   MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE |                                                     \
   MARU_WINDOW_ATTR_VISIBLE | MARU_WINDOW_ATTR_ICON)

/*
 * `title` and `surrounding_text` are copied by maru_createWindow() and
 * maru_updateWindow().
 *
 * `icon`, `cursor`, and `monitor` are borrowed same-context handles. They are
 * not retained by the window.
 *
 * `dip_position` updates may act as a silent no-op or return MARU_FAILURE on 
 * certain backends (like Wayland) where positioning is compositor-controlled.
 *
 * Constraint sentinels:
 * - `dip_max_size.x == 0` and/or `dip_max_size.y == 0` mean that axis is
 *   unbounded.
 * - `aspect_ratio = {0, 0}` disables the aspect-ratio constraint.
 * - Otherwise, `aspect_ratio.num` and `aspect_ratio.denom` must both be
 *   strictly positive.
 *
 * Window presentation state contract:
 * - `presentation_state` encodes the mutually exclusive modes
 *   `NORMAL`, `FULLSCREEN`, `MAXIMIZED`, and `MINIMIZED`.
 * - When the presentation state is set explicitly, Maru ensures that
 *   `fullscreen`/`maximized`/`minimized` mirror the selected mode.
 * - `FULLSCREEN` and `MAXIMIZED` require `visible == true`, `MINIMIZED`
 *   requires `visible == false`.
 * - `NORMAL` with `visible == false` represents a hidden but non-minimized
 *   window.
 *
 * `maru_updateWindow()` validates partial state updates against the window's
 * current requested state. When switching between these exclusive modes in one
 * step, update all affected state fields in the same call.
 *
 * `surrounding_text` is optional. When non-NULL, it must point to valid
 * NUL-terminated UTF-8, and `surrounding_cursor_byte` /
 * `surrounding_anchor_byte` must be byte offsets into that string landing on
 * UTF-8 code point boundaries. When `surrounding_text` is NULL, both offsets
 * must be 0, which clears the surrounding-text tuple.
 */
typedef struct MARU_WindowAttributes {
  const char* title;
  const MARU_Image* icon;

  MARU_Vec2Dip dip_size;
  MARU_Vec2Dip dip_position;
  MARU_Vec2Dip dip_viewport_size;
  const MARU_Monitor* monitor;
  MARU_Vec2Dip dip_min_size;
  MARU_Vec2Dip dip_max_size;
  MARU_Fraction aspect_ratio;

  bool visible;
  bool resizable;
  bool accept_drop;
  MARU_WindowPresentationState presentation_state;

  MARU_CursorMode cursor_mode;
  const MARU_Cursor* cursor;

  MARU_TextInputType text_input_type;
  MARU_RectDip dip_text_input_rect;
  const char* surrounding_text;
  uint32_t surrounding_cursor_byte;
  uint32_t surrounding_anchor_byte;
} MARU_WindowAttributes;

typedef struct MARU_WindowCreateInfo {
  MARU_WindowAttributes attributes;
  /*
   * Optional platform app identity. NULL leaves it unset.
   *
   * Copied by maru_createWindow(); it need only outlive that call.
   */
  const char* app_id;
  MARU_ContentType content_type;

  /*
   * Controls whether this window should have decorations at all.
   *
   * This is distinct from Wayland's context-level decoration strategy, which
   * chooses how Maru realizes decorated windows on that backend.
   *
   * Decoration presence cannot be changed after creation in all backends, so
   * this is a creation-time only property.
   */
  bool has_decorations;
  void* userdata;
} MARU_WindowCreateInfo;

#define MARU_WINDOW_CREATE_INFO_DEFAULT                         \
  {.attributes = {.title = "MARU Window",                       \
                  .icon = NULL,                                 \
                                                                \
                  .dip_size = {800, 600},                       \
                  .dip_position = {0, 0},                       \
                  .dip_viewport_size = {0, 0},                  \
                  .monitor = NULL,                              \
                  .dip_min_size = {0, 0},                       \
                  .dip_max_size = {0, 0},                       \
                  .aspect_ratio = {0, 0},                       \
                                                                \
                  .visible = true,                              \
                  .resizable = true,                            \
                  .accept_drop = false,                         \
                  .presentation_state = MARU_WINDOW_PRESENTATION_NORMAL, \
                  .cursor_mode = MARU_CURSOR_NORMAL,            \
                  .cursor = NULL,                               \
                                                                \
                  .text_input_type = MARU_TEXT_INPUT_TYPE_NONE, \
                  .dip_text_input_rect = {{0, 0}, {0, 0}},      \
                  .surrounding_text = NULL,                     \
                  .surrounding_cursor_byte = 0,                 \
                  .surrounding_anchor_byte = 0},                \
   .app_id = NULL,                                              \
   .content_type = MARU_CONTENT_TYPE_NONE,                      \
   .has_decorations = true,                                     \
   .userdata = NULL}


/*
 * The returned handle may exist before the native window is ready for use.
 * Wait for MARU_EVENT_WINDOW_READY or maru_isWindowReady(window) before using
 * APIs that require a realized native window, such as maru_updateWindow(),
 * maru_requestWindowFocus(), maru_requestWindowFrame(),
 * maru_requestWindowAttention(), maru_announceDragData(),
 * maru_requestDropData(), maru_getAvailableDropMIMETypes(), native window
 * getters, and Vulkan surface creation APIs declared in `maru/vulkan.h`.
 *
 * If the owning context is already lost, status-returning APIs on that window
 * short-circuit with MARU_CONTEXT_LOST before runtime ready-window
 * preconditions become relevant.
 */
MARU_API MARU_Status maru_createWindow(MARU_Context* context,
                                       const MARU_WindowCreateInfo* create_info,
                                       MARU_Window** out_window);
/*
 * Destroys a window and releases its backend resources.
 *
 * Unlike maru_destroyContext(), this is still a backend-facing teardown
 * request. If the owning context is already lost, it short-circuits with
 * MARU_CONTEXT_LOST; destroy the context itself for terminal cleanup.
 */
MARU_API MARU_Status maru_destroyWindow(MARU_Window* window);
/* Requires a ready window. */
MARU_API MARU_Status maru_updateWindow(MARU_Window* window,
                                       uint64_t field_mask,
                                       const MARU_WindowAttributes* attributes);
/* Requires a ready window. */
MARU_API MARU_Status maru_requestWindowFocus(MARU_Window* window);
/* Backends may coalesce redundant requests; treat this as an edge-triggered hint. */
/* Requires a ready window. */
MARU_API MARU_Status maru_requestWindowFrame(MARU_Window* window);
/* Requires a ready window. */
MARU_API MARU_Status maru_requestWindowAttention(MARU_Window* window);

/* ----- Controllers ----- */

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
   * Borrowed snapshot valid until the next maru_pumpEvents() call on the same
   * context. Retain handles you need to keep beyond that pump cycle.
   */
  MARU_Controller* const* controllers;
  uint32_t count;
} MARU_ControllerList;

/*
 * Lost-controller contract:
 *
 * A lost controller handle remains valid in memory until you release it.
 * Accessors and polling getters continue to operate on the last known
 * snapshot and may therefore return stale information after loss.
 * Backend-facing mutators become inert and return MARU_FAILURE once the
 * controller is lost.
 */
/* Userdata accessors follow the direct-inline-accessor threading rule. */
static inline void* maru_getControllerUserdata(const MARU_Controller* controller);
static inline void maru_setControllerUserdata(MARU_Controller* controller, void* userdata);
static inline const MARU_Context* maru_getControllerContext(const MARU_Controller* controller);
static inline bool maru_isControllerLost(const MARU_Controller* controller);
static inline const char* maru_getControllerName(const MARU_Controller* controller);
static inline uint16_t maru_getControllerVendorId(const MARU_Controller* controller);
static inline uint16_t maru_getControllerProductId(const MARU_Controller* controller);
static inline uint16_t maru_getControllerVersion(const MARU_Controller* controller);
/*
 * Returns the controller's stable 16-byte identifier by value.
 */
static inline MARU_Guid maru_getControllerGuid(const MARU_Controller* controller);
/*
 * When true, the first `MARU_CONTROLLER_*_STANDARD_COUNT` channels in each
 * controller state/channel-info array are guaranteed to be the Maru-defined
 * standardized channels in enum order. Extra channels, if any, follow after
 * that range.
 */
static inline bool maru_isControllerStandardized(const MARU_Controller* controller);
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

MARU_API MARU_Status maru_getControllers(const MARU_Context* context, MARU_ControllerList* out_list);
MARU_API void maru_retainController(MARU_Controller* controller);
MARU_API void maru_releaseController(MARU_Controller* controller);
/*
 * Each intensity must fall within the inclusive `[min_value, max_value]` range
 * published by `maru_getControllerHapticChannelInfo()` for the targeted
 * haptic channels. Standard Maru haptics use 0..1.
 */
MARU_API MARU_Status maru_setControllerHapticLevels(MARU_Controller* controller,
                                                    uint32_t first_haptic,
                                                    uint32_t count,
                                                    const MARU_Scalar* intensities);

/* ----- Data exchange ----- */

/*
 * Announces clipboard ownership for the context.
 *
 * Clipboard events associated with this ownership flow are delivered as
 * context-scoped callbacks (`window == NULL`).
 *
 * `mime_types` strings are copied before a successful return.
 */
MARU_API MARU_Status maru_announceClipboardData(MARU_Context* context,
                                                MARU_StringList mime_types);
/* `mime_types` strings are copied before a successful return. */
/* Requires a ready window. */
MARU_API MARU_Status maru_announceDragData(MARU_Window* window,
                                           MARU_StringList mime_types,
                                           MARU_DropActionMask allowed_actions);
/*
 * Fulfills a callback-scoped clipboard request delivered through
 * MARU_EVENT_DATA_REQUESTED.
 *
 * If `flags` contains MARU_DATA_PROVIDE_FLAG_ZERO_COPY, `data` must remain
 * readable until the matching MARU_EVENT_DATA_RELEASED callback fires or the
 * owning context is destroyed. Otherwise, the payload is copied before the API
 * returns successfully.
 */
MARU_API MARU_Status maru_provideClipboardData(MARU_DataRequest* request,
                                               const void* data,
                                               size_t size,
                                               MARU_DataProvideFlags flags);
/*
 * Fulfills a callback-scoped drag/drop request delivered through
 * MARU_EVENT_DATA_REQUESTED.
 *
 * If `flags` contains MARU_DATA_PROVIDE_FLAG_ZERO_COPY, `data` must remain
 * readable until the matching MARU_EVENT_DATA_RELEASED callback fires or the
 * owning context is destroyed. Otherwise, the payload is copied before the API
 * returns successfully.
 */
MARU_API MARU_Status maru_provideDropData(MARU_DataRequest* request,
                                          const void* data,
                                          size_t size,
                                          MARU_DataProvideFlags flags);
/*
 * Requests clipboard data for the context.
 *
 * Clipboard events associated with this request flow are delivered as
 * context-scoped callbacks (`window == NULL`).
 *
 * `mime_type` is copied before maru_requestClipboardData() returns
 * successfully.
 *
 * `userdata` is not interpreted by Maru. On completion, the matching
 * MARU_EVENT_DATA_RECEIVED callback reports the same pointer in
 * event->data_received.userdata.
 */
MARU_API MARU_Status maru_requestClipboardData(MARU_Context* context,
                                               const char* mime_type,
                                               void* userdata);
/*
 * `mime_type` is copied before maru_requestDropData() returns successfully.
 *
 * `userdata` is not interpreted by Maru. On completion, the matching
 * MARU_EVENT_DATA_RECEIVED callback reports the same pointer in
 * event->data_received.userdata.
 */
/* Requires a ready window. */
MARU_API MARU_Status maru_requestDropData(MARU_Window* window,
                                          const char* mime_type,
                                          void* userdata);
/*
 * Returns a borrowed snapshot of the currently available clipboard MIME types.
 * The snapshot is invalidated by the next
 * maru_getAvailableClipboardMIMETypes() call on the same context, and may also
 * be replaced by a later pump cycle that changes the clipboard offer.
 */
MARU_API MARU_Status maru_getAvailableClipboardMIMETypes(const MARU_Context* context,
                                                         MARU_StringList* out_list);
/*
 * Returns a borrowed snapshot of the currently available drop-offer MIME
 * types for `window`. The snapshot is invalidated by the next
 * maru_getAvailableDropMIMETypes() call on the same window, and may also be
 * replaced by a later pump cycle that changes that window's active drop offer.
 */
/* Requires a ready window. */
MARU_API MARU_Status maru_getAvailableDropMIMETypes(const MARU_Window* window,
                                                    MARU_StringList* out_list);

/*
 * Drop-session helpers operate on callback-scoped handles delivered through
 * MARU_EVENT_DROP_ENTERED / HOVERED / EXITED / DROPPED.
 *
 * The handle itself is only valid for the duration of the callback that
 * delivered it and must not be stored.
 *
 * maru_setDropSessionUserdata() / maru_getDropSessionUserdata() let you attach
 * application state to the underlying logical drag session so later DnD
 * callbacks in that same session can recover it through their own callback-
 * scoped handle.
 *
 * maru_getDropSessionAction() reports the current target-side selected action
 * for that callback-scoped session handle. During
 * MARU_EVENT_DROP_ENTERED / HOVERED, maru_setDropSessionAction() overrides the
 * action Maru will report back to the backend for that callback. During
 * MARU_EVENT_DROP_EXITED / DROPPED, the getter reports the last known selected
 * action for the session; setting it there has no defined effect.
 *
 * maru_setDropSessionAction() is only meaningful during
 * MARU_EVENT_DROP_ENTERED and MARU_EVENT_DROP_HOVERED. The selected action is
 * consumed by the backend before that callback returns. The supplied action
 * must be MARU_DROP_ACTION_NONE or exactly one of COPY / MOVE / LINK, and any
 * non-NONE choice must be present in the callback's available_actions mask.
 * Calling it during
 * MARU_EVENT_DROP_EXITED or MARU_EVENT_DROP_DROPPED has no defined effect.
 */
static inline void maru_setDropSessionAction(MARU_DropSession* session, MARU_DropAction action);
static inline MARU_DropAction maru_getDropSessionAction(const MARU_DropSession* session);
static inline void maru_setDropSessionUserdata(MARU_DropSession* session, void* userdata);
static inline void* maru_getDropSessionUserdata(const MARU_DropSession* session);

/* ----- Convenience helpers ----- */

static inline MARU_Status maru_setContextInhibitsSystemIdle(MARU_Context* context, bool enabled);
static inline MARU_Status maru_setContextIdleTimeout(MARU_Context* context, uint32_t timeout_ms);
static inline MARU_Status maru_setContextDiagnosticCallback(MARU_Context* context, MARU_DiagnosticCallback cb, void* userdata);
static inline MARU_Status maru_setWindowTitle(MARU_Window* window, const char* title);
static inline MARU_Status maru_setWindowDipSize(MARU_Window* window, MARU_Vec2Dip size);
static inline MARU_Status maru_setWindowDipViewportSize(MARU_Window* window, MARU_Vec2Dip size);
/* Note: Window positioning is compositor-controlled on certain backends (like Wayland), where this will act as a silent no-op or return MARU_FAILURE. */
static inline MARU_Status maru_setWindowDipPosition(MARU_Window* window, MARU_Vec2Dip position);
/*
 * Convenience window setters update only the named attribute.
 *
 * For coordinated presentation-state transitions involving multiple fields
 * (`visible`, `minimized`, `maximized`, `fullscreen`), use maru_updateWindow()
 * directly and submit all affected fields in one call.
 */
static inline MARU_Status maru_setWindowFullscreen(MARU_Window* window, bool enabled);
static inline MARU_Status maru_setWindowMaximized(MARU_Window* window, bool enabled);
static inline MARU_Status maru_setWindowCursorMode(MARU_Window* window, MARU_CursorMode mode);
static inline MARU_Status maru_setWindowCursor(MARU_Window* window, const MARU_Cursor* cursor);
static inline MARU_Status maru_setWindowMonitor(MARU_Window* window, const MARU_Monitor* monitor);
static inline MARU_Status maru_setWindowDipMinSize(MARU_Window* window, MARU_Vec2Dip dip_min_size);
static inline MARU_Status maru_setWindowDipMaxSize(MARU_Window* window, MARU_Vec2Dip dip_max_size);
static inline MARU_Status maru_setWindowAspectRatio(MARU_Window* window,
                                                    MARU_Fraction aspect_ratio);
static inline MARU_Status maru_setWindowResizable(MARU_Window* window, bool enabled);
static inline MARU_Status maru_setWindowTextInputType(MARU_Window* window, MARU_TextInputType type);
static inline MARU_Status maru_setWindowDipTextInputRect(MARU_Window* window, MARU_RectDip rect);
static inline MARU_Status maru_setWindowSurroundingText(MARU_Window* window, const char* text, uint32_t cursor_byte, uint32_t anchor_byte);
static inline MARU_Status maru_setWindowAcceptDrop(MARU_Window* window, bool enabled);
static inline MARU_Status maru_setWindowVisible(MARU_Window* window, bool visible);
static inline MARU_Status maru_setWindowMinimized(MARU_Window* window, bool minimized);
static inline MARU_Status maru_setWindowIcon(MARU_Window* window, const MARU_Image* icon);
/*
 * Returns a stable diagnostic label when diagnostics are compiled in.
 *
 * When MARU_ENABLE_DIAGNOSTICS is disabled, diagnostic string literals are
 * compiled out and this returns an empty string.
 */
MARU_API const char* maru_getDiagnosticString(MARU_Diagnostic diagnostic);

/* ----- Inline accessors and helper implementations ----- */

#define MARU__INCLUDING_DETAILS_FROM_MARU_H 1
#include "maru/details/maru_details.h"
#undef MARU__INCLUDING_DETAILS_FROM_MARU_H

#ifdef __cplusplus
}
#endif

#endif
