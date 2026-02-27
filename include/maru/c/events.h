// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_EVENTS_H_INCLUDED
#define MARU_EVENTS_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/geometry.h"
#include "maru/c/inputs.h"

/**
 * @file events.h
 * @brief Standard event payloads and event dispatching APIs.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing the library state and display server connection. */
typedef struct MARU_Context MARU_Context;
/** @brief Opaque handle representing an OS-level window. */
typedef struct MARU_Window MARU_Window;
/** @brief Opaque handle representing a physical monitor. */
typedef struct MARU_Monitor MARU_Monitor;
/** @brief Opaque handle representing a physical controller. */
typedef struct MARU_Controller MARU_Controller;

/** @brief Identifies one specific event kind delivered to callbacks. */
typedef enum MARU_EventId {
  MARU_EVENT_CLOSE_REQUESTED = 0,
  MARU_EVENT_WINDOW_RESIZED = 1,
  MARU_EVENT_KEY_STATE_CHANGED = 2,
  MARU_EVENT_WINDOW_READY = 3,
  MARU_EVENT_MOUSE_MOVED = 4,
  MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED = 5,
  MARU_EVENT_MOUSE_SCROLLED = 6,
  MARU_EVENT_IDLE_STATE_CHANGED = 7,
  MARU_EVENT_MONITOR_CONNECTION_CHANGED = 8,
  MARU_EVENT_MONITOR_MODE_CHANGED = 9,
  MARU_EVENT_WINDOW_FRAME = 10,
  MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED = 11,
  MARU_EVENT_TEXT_EDIT_START = 12,
  MARU_EVENT_TEXT_EDIT_UPDATE = 13,
  MARU_EVENT_TEXT_EDIT_COMMIT = 14,
  MARU_EVENT_TEXT_EDIT_END = 15,

  /* Data Exchange (32-39) */
  MARU_EVENT_DROP_ENTERED = 16,
  MARU_EVENT_DROP_HOVERED = 17,
  MARU_EVENT_DROP_EXITED = 18,
  MARU_EVENT_DROP_DROPPED = 19,
  MARU_EVENT_DATA_RECEIVED = 20,
  MARU_EVENT_DATA_REQUESTED = 21,
  MARU_EVENT_DATA_CONSUMED = 22,
  MARU_EVENT_DRAG_FINISHED = 23,

  /* Controllers (46-47) */
  MARU_EVENT_CONTROLLER_CONNECTION_CHANGED = 24,
  MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED = 25,

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

#define MARU_EVENT_MASK(id) ((MARU_EventMask)1ULL << (uint32_t)(id))

// N.B. static const MARU_EventMask is the best way to represent 
// this in a stable manner, but it can trigger noisy warnings.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif

/** @brief Event fired when a window close request is received. */
static const MARU_EventMask MARU_MASK_CLOSE_REQUESTED = MARU_EVENT_MASK(MARU_EVENT_CLOSE_REQUESTED);
/** @brief Event fired when window geometry changes (size/scale/transform). */
static const MARU_EventMask MARU_MASK_WINDOW_RESIZED = MARU_EVENT_MASK(MARU_EVENT_WINDOW_RESIZED);
/** @brief Event fired when a keyboard key state changes. */
static const MARU_EventMask MARU_MASK_KEY_STATE_CHANGED = MARU_EVENT_MASK(MARU_EVENT_KEY_STATE_CHANGED);
/** @brief Event fired when a window is fully created and ready for interaction. */
static const MARU_EventMask MARU_MASK_WINDOW_READY = MARU_EVENT_MASK(MARU_EVENT_WINDOW_READY);
/** @brief Event fired when the mouse cursor moves. */
static const MARU_EventMask MARU_MASK_MOUSE_MOVED = MARU_EVENT_MASK(MARU_EVENT_MOUSE_MOVED);
/** @brief Event fired when a mouse button state changes. */
static const MARU_EventMask MARU_MASK_MOUSE_BUTTON_STATE_CHANGED = MARU_EVENT_MASK(MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED);
/** @brief Event fired when a mouse scroll wheel is moved. */
static const MARU_EventMask MARU_MASK_MOUSE_SCROLLED = MARU_EVENT_MASK(MARU_EVENT_MOUSE_SCROLLED);
/** @brief Event fired when the system idle state changes. */
static const MARU_EventMask MARU_MASK_IDLE_STATE_CHANGED = MARU_EVENT_MASK(MARU_EVENT_IDLE_STATE_CHANGED);
/** @brief Event fired when a monitor is connected or disconnected. */
static const MARU_EventMask MARU_MASK_MONITOR_CONNECTION_CHANGED = MARU_EVENT_MASK(MARU_EVENT_MONITOR_CONNECTION_CHANGED);
/** @brief Event fired when a monitor's display mode changes. */
static const MARU_EventMask MARU_MASK_MONITOR_MODE_CHANGED = MARU_EVENT_MASK(MARU_EVENT_MONITOR_MODE_CHANGED);
/** @brief Event fired when the window should render its next frame. */
static const MARU_EventMask MARU_MASK_WINDOW_FRAME = MARU_EVENT_MASK(MARU_EVENT_WINDOW_FRAME);
/** @brief Event fired when a window presentation state changes. */
static const MARU_EventMask MARU_MASK_WINDOW_PRESENTATION_STATE_CHANGED =
    MARU_EVENT_MASK(MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED);

/** @brief Event fired when an IME composition session starts. */
static const MARU_EventMask MARU_MASK_TEXT_EDIT_START = MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_START);
/** @brief Event fired when the current preedit text in an IME session is updated. */
static const MARU_EventMask MARU_MASK_TEXT_EDIT_UPDATE = MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_UPDATE);
/** @brief Event fired when text is committed to the application, potentially replacing surrounding text. */
static const MARU_EventMask MARU_MASK_TEXT_EDIT_COMMIT = MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_COMMIT);
/** @brief Event fired when an IME composition session ends. */
static const MARU_EventMask MARU_MASK_TEXT_EDIT_END = MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_END);

  // N.B. These bits are reserved for user-defined events
static const MARU_EventMask MARU_MASK_USER_0 = MARU_EVENT_MASK(MARU_EVENT_USER_0);
static const MARU_EventMask MARU_MASK_USER_1 = MARU_EVENT_MASK(MARU_EVENT_USER_1);
static const MARU_EventMask MARU_MASK_USER_2 = MARU_EVENT_MASK(MARU_EVENT_USER_2);
static const MARU_EventMask MARU_MASK_USER_3 = MARU_EVENT_MASK(MARU_EVENT_USER_3);
static const MARU_EventMask MARU_MASK_USER_4 = MARU_EVENT_MASK(MARU_EVENT_USER_4);
static const MARU_EventMask MARU_MASK_USER_5 = MARU_EVENT_MASK(MARU_EVENT_USER_5);
static const MARU_EventMask MARU_MASK_USER_6 = MARU_EVENT_MASK(MARU_EVENT_USER_6);
static const MARU_EventMask MARU_MASK_USER_7 = MARU_EVENT_MASK(MARU_EVENT_USER_7);
static const MARU_EventMask MARU_MASK_USER_8 = MARU_EVENT_MASK(MARU_EVENT_USER_8);
static const MARU_EventMask MARU_MASK_USER_9 = MARU_EVENT_MASK(MARU_EVENT_USER_9);
static const MARU_EventMask MARU_MASK_USER_10 = MARU_EVENT_MASK(MARU_EVENT_USER_10);
static const MARU_EventMask MARU_MASK_USER_11 = MARU_EVENT_MASK(MARU_EVENT_USER_11);
static const MARU_EventMask MARU_MASK_USER_12 = MARU_EVENT_MASK(MARU_EVENT_USER_12);
static const MARU_EventMask MARU_MASK_USER_13 = MARU_EVENT_MASK(MARU_EVENT_USER_13);
static const MARU_EventMask MARU_MASK_USER_14 = MARU_EVENT_MASK(MARU_EVENT_USER_14);
static const MARU_EventMask MARU_MASK_USER_15 = MARU_EVENT_MASK(MARU_EVENT_USER_15);

/** @brief Bitmask representing all possible event types. */
static const MARU_EventMask MARU_ALL_EVENTS = ~((MARU_EventMask)0);

/** @brief Returns true if the provided event id is in the user-reserved range. */
static inline bool maru_isUserEventId(MARU_EventId id) {
  return id >= MARU_EVENT_USER_0 && id <= MARU_EVENT_USER_15;
}

/** @brief Returns true if the provided event id is recognized by MARU. */
static inline bool maru_isKnownEventId(MARU_EventId id) {
  switch (id) {
    case MARU_EVENT_CLOSE_REQUESTED:
    case MARU_EVENT_WINDOW_RESIZED:
    case MARU_EVENT_KEY_STATE_CHANGED:
    case MARU_EVENT_WINDOW_READY:
    case MARU_EVENT_MOUSE_MOVED:
    case MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED:
    case MARU_EVENT_MOUSE_SCROLLED:
    case MARU_EVENT_IDLE_STATE_CHANGED:
    case MARU_EVENT_MONITOR_CONNECTION_CHANGED:
    case MARU_EVENT_MONITOR_MODE_CHANGED:
    case MARU_EVENT_WINDOW_FRAME:
    case MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED:
    case MARU_EVENT_TEXT_EDIT_START:
    case MARU_EVENT_TEXT_EDIT_UPDATE:
    case MARU_EVENT_TEXT_EDIT_COMMIT:
    case MARU_EVENT_TEXT_EDIT_END:
    case MARU_EVENT_DROP_ENTERED:
    case MARU_EVENT_DROP_HOVERED:
    case MARU_EVENT_DROP_EXITED:
    case MARU_EVENT_DROP_DROPPED:
    case MARU_EVENT_DATA_RECEIVED:
    case MARU_EVENT_DATA_REQUESTED:
    case MARU_EVENT_DATA_CONSUMED:
    case MARU_EVENT_DRAG_FINISHED:
    case MARU_EVENT_CONTROLLER_CONNECTION_CHANGED:
    case MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED:
      return true;
    default:
      return maru_isUserEventId(id);
  }
}

/** @brief Returns true when the given event id bit is enabled in a mask. */
static inline bool maru_eventMaskHas(MARU_EventMask mask, MARU_EventId id) {
  return (mask & MARU_EVENT_MASK(id)) != 0;
}

/** @brief Returns a new mask with the given event id bit enabled. */
static inline MARU_EventMask maru_eventMaskAdd(MARU_EventMask mask, MARU_EventId id) {
  return mask | MARU_EVENT_MASK(id);
}

/** @brief Returns a new mask with the given event id bit disabled. */
static inline MARU_EventMask maru_eventMaskRemove(MARU_EventMask mask, MARU_EventId id) {
  return mask & ~MARU_EVENT_MASK(id);
}

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* IMPORTANT:
 Every pointer in events at every layer of indirection is to be treated
 as only valid for the duration of the callback.
*/

/** @brief Payload for MARU_EVENT_WINDOW_READY. */
typedef struct MARU_WindowReadyEvent {
  MARU_WindowGeometry geometry;
} MARU_WindowReadyEvent;
/** @brief Payload for MARU_EVENT_CLOSE_REQUESTED. */
typedef struct MARU_WindowCloseEvent { char _unused; } MARU_WindowCloseEvent;
/** @brief Payload for MARU_EVENT_WINDOW_FRAME. */
typedef struct MARU_WindowFrameEvent { 
  uint32_t timestamp_ms; 
} MARU_WindowFrameEvent;

/** @brief Payload for MARU_EVENT_WINDOW_RESIZED. */
typedef struct MARU_WindowResizedEvent {
  MARU_WindowGeometry geometry;
} MARU_WindowResizedEvent;

/** @brief Bitmask describing which presentation fields changed. */
typedef enum MARU_WindowPresentationChangedBits {
  MARU_WINDOW_PRESENTATION_CHANGED_VISIBLE = 1u << 0,
  MARU_WINDOW_PRESENTATION_CHANGED_MINIMIZED = 1u << 1,
  MARU_WINDOW_PRESENTATION_CHANGED_MAXIMIZED = 1u << 2,
  MARU_WINDOW_PRESENTATION_CHANGED_FOCUSED = 1u << 3,
  MARU_WINDOW_PRESENTATION_CHANGED_ICON = 1u << 4,
} MARU_WindowPresentationChangedBits;

/** @brief Payload for MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED. */
typedef struct MARU_WindowPresentationStateEvent {
  uint32_t changed_fields;
  bool visible;
  bool minimized;
  bool maximized;
  bool focused;
  bool icon_effective;
} MARU_WindowPresentationStateEvent;

/** @brief Payload for MARU_EVENT_KEY_STATE_CHANGED. */
// N.B. This represents raw keyboard data.
// - It does not get fired on keyrepeats
// - the key will likely not match the current keyboard layout
// - If you are using this for text, use MARU_EVENT_TEXT_EDIT_COMMIT instead
typedef struct MARU_KeyboardEvent {
  MARU_Key raw_key;
  MARU_ButtonState state;
  MARU_ModifierFlags modifiers;
} MARU_KeyboardEvent;

/** @brief Payload for MARU_EVENT_MOUSE_MOVED. */
typedef struct MARU_MouseMotionEvent {
  MARU_Vec2Dip position;
  MARU_Vec2Dip delta;
  MARU_Vec2Dip raw_delta;
  MARU_ModifierFlags modifiers;
} MARU_MouseMotionEvent;

/** @brief Payload for MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED. */
typedef struct MARU_MouseButtonEvent {
  /** Index into maru_getMouseButtonStates()/maru_getMouseButtonChannelInfo(). */
  uint32_t button_id;
  MARU_ButtonState state;
  MARU_ModifierFlags modifiers;
} MARU_MouseButtonEvent;

/** @brief Payload for MARU_EVENT_MOUSE_SCROLLED. */
typedef struct MARU_MouseScrollEvent {
  MARU_Vec2Dip delta;
  struct { int32_t x; int32_t y; } steps;
  MARU_ModifierFlags modifiers;
} MARU_MouseScrollEvent;

/** @brief Payload for MARU_EVENT_IDLE_STATE_CHANGED. */
typedef struct MARU_IdleEvent {
  uint32_t timeout_ms;
  bool is_idle;
} MARU_IdleEvent;

/** @brief Payload for MARU_EVENT_MONITOR_CONNECTION_CHANGED. */
typedef struct MARU_MonitorConnectionEvent {
  MARU_Monitor *monitor;
  bool connected;
} MARU_MonitorConnectionEvent;

/** @brief Payload for MARU_EVENT_MONITOR_MODE_CHANGED. */
typedef struct MARU_MonitorModeEvent {
  MARU_Monitor *monitor;
} MARU_MonitorModeEvent;

/** @brief Supported Drag and Drop actions for OS feedback. */
typedef enum MARU_DropAction {
  MARU_DROP_ACTION_NONE = 0,
  MARU_DROP_ACTION_COPY = 1 << 0,
  MARU_DROP_ACTION_MOVE = 1 << 1,
  MARU_DROP_ACTION_LINK = 1 << 2,
} MARU_DropAction;

/** @brief Bitmask of allowed drop actions. */
typedef uint32_t MARU_DropActionMask;

/** @brief System-managed data exchange targets. */
typedef enum MARU_DataExchangeTarget {
  MARU_DATA_EXCHANGE_TARGET_CLIPBOARD = 0,
  MARU_DATA_EXCHANGE_TARGET_PRIMARY = 1,
  MARU_DATA_EXCHANGE_TARGET_DRAG_DROP = 2,
} MARU_DataExchangeTarget;

/** @brief A transient list of MIME type strings. */
typedef struct MARU_MIMETypeList {
  const char *const *mime_types;
  uint32_t count;
} MARU_MIMETypeList;

/** @brief Flags that influence how data is provided for a request. */
typedef enum MARU_DataProvideFlags {
  MARU_DATA_PROVIDE_FLAG_NONE = 0,
  MARU_DATA_PROVIDE_FLAG_ZERO_COPY = 1 << 0,
} MARU_DataProvideFlags;

/** @brief Controls application response to a drag-and-drop hover. */
typedef struct MARU_DropFeedback {
  MARU_DropAction *action;
  void **session_userdata;
} MARU_DropFeedback;

/** @brief Payload for MARU_EVENT_DROP_ENTERED. */
typedef struct MARU_DropEnterEvent {
  MARU_Vec2Dip position;
  MARU_DropFeedback *feedback;
  const char **paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint16_t path_count;
} MARU_DropEnterEvent;

/** @brief Payload for MARU_EVENT_DROP_HOVERED. */
typedef struct MARU_DropHoverEvent {
  MARU_Vec2Dip position;
  MARU_DropFeedback *feedback;
  const char **paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint16_t path_count;
} MARU_DropHoverEvent;

/** @brief Payload for MARU_EVENT_DROP_EXITED. */
typedef struct MARU_DropLeaveEvent {
  void **session_userdata;
} MARU_DropLeaveEvent;

/** @brief Payload for MARU_EVENT_DROP_DROPPED. */
typedef struct MARU_DropEvent {
  MARU_Vec2Dip position;
  void **session_userdata;
  const char **paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint16_t path_count;
} MARU_DropEvent;

/** @brief Payload for MARU_EVENT_DATA_RECEIVED. */
typedef struct MARU_DataReceivedEvent {
  void *user_tag;
  MARU_Status status;
  MARU_DataExchangeTarget target;
  const char *mime_type;
  const void *data;
  size_t size;
} MARU_DataReceivedEvent;

/** @brief Payload for MARU_EVENT_DATA_REQUESTED. */
typedef struct MARU_DataRequestEvent {
  MARU_DataExchangeTarget target;
  const char *mime_type;
  void *internal_handle;
} MARU_DataRequestEvent;

/** @brief Payload for MARU_EVENT_DATA_CONSUMED. */
typedef struct MARU_DataConsumedEvent {
  MARU_DataExchangeTarget target;
  const char *mime_type;
  const void *data;
  size_t size;
  MARU_DropAction action;
} MARU_DataConsumedEvent;

/** @brief Payload for MARU_EVENT_DRAG_FINISHED. */
typedef struct MARU_DragFinishedEvent {
  MARU_DropAction action;
} MARU_DragFinishedEvent;

/** @brief Event payload for controller hotplug changes. */
typedef struct MARU_ControllerConnectionEvent {
  MARU_Controller *controller;
  bool connected;
} MARU_ControllerConnectionEvent;

/** @brief Event payload for controller button state changes. */
typedef struct MARU_ControllerButtonStateChangedEvent {
  MARU_Controller *controller;
  MARU_ButtonState state;
  uint32_t button_id;
} MARU_ControllerButtonStateChangedEvent;

/** @brief UTF-8 byte range. */
typedef struct MARU_TextRangeUtf8 {
  uint32_t start_byte;
  uint32_t length_byte;
} MARU_TextRangeUtf8;

/** @brief Payload for MARU_EVENT_TEXT_EDIT_START. */
typedef struct MARU_TextEditStartEvent {
  uint64_t session_id;
} MARU_TextEditStartEvent;

/** @brief Payload for MARU_EVENT_TEXT_EDIT_UPDATE. */
typedef struct MARU_TextEditUpdateEvent {
  uint64_t session_id;
  const char *preedit_utf8;
  uint32_t preedit_length;
  MARU_TextRangeUtf8 caret;
  MARU_TextRangeUtf8 selection;
} MARU_TextEditUpdateEvent;

/** @brief Payload for MARU_EVENT_TEXT_EDIT_COMMIT.
 *
 * - `delete_before_bytes` and `delete_after_bytes` are UTF-8 byte counts around
 *   the application's current insertion point (cursor), not codepoint counts.
 * - `committed_utf8` points to transient UTF-8 data valid only during the event
 *   callback. It may be NULL only when `committed_length == 0`.
 * - `committed_length` is the byte length and may be 0 for pure deletion commits.
 */
typedef struct MARU_TextEditCommitEvent {
  uint64_t session_id;
  uint32_t delete_before_bytes;
  uint32_t delete_after_bytes;
  const char *committed_utf8;
  uint32_t committed_length;
} MARU_TextEditCommitEvent;

/** @brief Payload for MARU_EVENT_TEXT_EDIT_END. */
typedef struct MARU_TextEditEndEvent {
  uint64_t session_id;
  bool canceled;
} MARU_TextEditEndEvent;

/** @brief Payload for user-defined events. */
typedef struct MARU_UserDefinedEvent {
  union {
    void* userdata;
    char raw_payload[32];
  };
} MARU_UserDefinedEvent;

/** @brief Unified standard event payload union. */
typedef struct MARU_Event {
  union {
    MARU_WindowReadyEvent window_ready;
    MARU_WindowCloseEvent close_requested;
    MARU_WindowResizedEvent resized;
    MARU_WindowPresentationStateEvent presentation;
    MARU_KeyboardEvent key;
    MARU_MouseMotionEvent mouse_motion;
    MARU_MouseButtonEvent mouse_button;
    MARU_MouseScrollEvent mouse_scroll;
    MARU_IdleEvent idle;
    MARU_MonitorConnectionEvent monitor_connection;
    MARU_MonitorModeEvent monitor_mode;
    MARU_DropEnterEvent drop_enter;
    MARU_DropHoverEvent drop_hover;
    MARU_DropLeaveEvent drop_leave;
    MARU_DropEvent drop;
    MARU_DataReceivedEvent data_received;
    MARU_DataRequestEvent data_requested;
    MARU_DataConsumedEvent data_consumed;
    MARU_DragFinishedEvent drag_finished;
    MARU_ControllerConnectionEvent controller_connection;
    MARU_ControllerButtonStateChangedEvent controller_button_state_changed;
    MARU_WindowFrameEvent frame;
    MARU_TextEditStartEvent text_edit_start;
    MARU_TextEditUpdateEvent text_edit_update;
    MARU_TextEditCommitEvent text_edit_commit;
    MARU_TextEditEndEvent text_edit_end;

    MARU_UserDefinedEvent user;
  };
} MARU_Event;

/** @brief Callback signature for event dispatching. */
typedef void (*MARU_EventCallback)(MARU_EventId type, MARU_Window *window, const struct MARU_Event *evt, void *userdata);

/** @brief Synchronizes the event queue with the OS and dispatches events to the provided callback. */
MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata);

/** @brief Manually pushes a user-defined event into the queue.
 *
 *  This is a GLOBALLY THREAD-SAFE function and can be called from any thread 
 *  without external synchronization. The passed event will be COPIED.
 */
MARU_Status maru_postEvent(MARU_Context *context, MARU_EventId type,
                                     MARU_Window *window, MARU_UserDefinedEvent evt);

/** @brief Wakes a context up without dispatching an event. 
 *
 *  This is a GLOBALLY THREAD-SAFE function and can be called from any thread 
 *  without external synchronization.
 */
MARU_Status maru_wakeContext(MARU_Context *context);

#ifdef __cplusplus
}
#endif

#endif  // MARU_EVENTS_H_INCLUDED
