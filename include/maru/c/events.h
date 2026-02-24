// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_EVENTS_H_INCLUDED
#define MARU_EVENTS_H_INCLUDED

#include <stdbool.h>
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


// N.B. static const MARU_EventMask is the best way to represent 
// this in a stable manner, but it can trigger noisy warnings.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif

/** @brief Event fired when a window close request is received. */
static const MARU_EventMask MARU_CLOSE_REQUESTED = ((MARU_EventMask)1 << 0);
/** @brief Event fired when a window has been resized. */
static const MARU_EventMask MARU_WINDOW_RESIZED = ((MARU_EventMask)1 << 1);
/** @brief Event fired when a keyboard key state changes. */
static const MARU_EventMask MARU_KEY_STATE_CHANGED = ((MARU_EventMask)1 << 2);
/** @brief Event fired when a window is fully created and ready for interaction. */
static const MARU_EventMask MARU_WINDOW_READY = ((MARU_EventMask)1 << 3);
/** @brief Event fired when the mouse cursor moves. */
static const MARU_EventMask MARU_MOUSE_MOVED = ((MARU_EventMask)1 << 4);
/** @brief Event fired when a mouse button state changes. */
static const MARU_EventMask MARU_MOUSE_BUTTON_STATE_CHANGED = ((MARU_EventMask)1 << 5);
/** @brief Event fired when a mouse scroll wheel is moved. */
static const MARU_EventMask MARU_MOUSE_SCROLLED = ((MARU_EventMask)1 << 6);
/** @brief Event fired when the system idle state changes. */
static const MARU_EventMask MARU_IDLE_STATE_CHANGED = ((MARU_EventMask)1 << 7);
/** @brief Event fired when a monitor is connected or disconnected. */
static const MARU_EventMask MARU_MONITOR_CONNECTION_CHANGED = ((MARU_EventMask)1 << 8);
/** @brief Event fired when a monitor's display mode changes. */
static const MARU_EventMask MARU_MONITOR_MODE_CHANGED = ((MARU_EventMask)1 << 9);
/** @brief Event fired when the window should render its next frame. */
static const MARU_EventMask MARU_WINDOW_FRAME = ((MARU_EventMask)1 << 10);
/** @brief Event fired when Unicode text input is received. 
 *  @deprecated Use MARU_TEXT_EDIT_COMMIT instead.
 */
static const MARU_EventMask MARU_TEXT_INPUT_RECEIVED = ((MARU_EventMask)1 << 11);
/** @brief Event fired when a window gains or loses focus. */
static const MARU_EventMask MARU_FOCUS_CHANGED = ((MARU_EventMask)1 << 12);
/** @brief Event fired when a window is maximized or restored. */
static const MARU_EventMask MARU_WINDOW_MAXIMIZED = ((MARU_EventMask)1 << 13);
/** @brief Event fired when IME text composition state changes. 
 *  @deprecated Use MARU_TEXT_EDIT_UPDATE instead.
 */
static const MARU_EventMask MARU_TEXT_COMPOSITION_UPDATED = ((MARU_EventMask)1 << 18);

/** @brief Event fired when an IME composition session starts. */
static const MARU_EventMask MARU_TEXT_EDIT_START = ((MARU_EventMask)1 << 19);
/** @brief Event fired when the current preedit text in an IME session is updated. */
static const MARU_EventMask MARU_TEXT_EDIT_UPDATE = ((MARU_EventMask)1 << 20);
/** @brief Event fired when text is committed to the application, potentially replacing surrounding text. */
static const MARU_EventMask MARU_TEXT_EDIT_COMMIT = ((MARU_EventMask)1 << 21);
/** @brief Event fired when an IME composition session ends. */
static const MARU_EventMask MARU_TEXT_EDIT_END = ((MARU_EventMask)1 << 22);

  // N.B. These bits are reserved for user-defined events
static const MARU_EventMask MARU_USER_EVENT_0 = ((MARU_EventMask)1 << 48);
static const MARU_EventMask MARU_USER_EVENT_1 = ((MARU_EventMask)1 << 49);
static const MARU_EventMask MARU_USER_EVENT_2 = ((MARU_EventMask)1 << 50);
static const MARU_EventMask MARU_USER_EVENT_3 = ((MARU_EventMask)1 << 51);
static const MARU_EventMask MARU_USER_EVENT_4 = ((MARU_EventMask)1 << 52);
static const MARU_EventMask MARU_USER_EVENT_5 = ((MARU_EventMask)1 << 53);
static const MARU_EventMask MARU_USER_EVENT_6 = ((MARU_EventMask)1 << 54);
static const MARU_EventMask MARU_USER_EVENT_7 = ((MARU_EventMask)1 << 55);
static const MARU_EventMask MARU_USER_EVENT_8 = ((MARU_EventMask)1 << 56);
static const MARU_EventMask MARU_USER_EVENT_9 = ((MARU_EventMask)1 << 57);
static const MARU_EventMask MARU_USER_EVENT_10 = ((MARU_EventMask)1 << 58);
static const MARU_EventMask MARU_USER_EVENT_11 = ((MARU_EventMask)1 << 59);
static const MARU_EventMask MARU_USER_EVENT_12 = ((MARU_EventMask)1 << 60);
static const MARU_EventMask MARU_USER_EVENT_13 = ((MARU_EventMask)1 << 61);
static const MARU_EventMask MARU_USER_EVENT_14 = ((MARU_EventMask)1 << 62);
static const MARU_EventMask MARU_USER_EVENT_15 = ((MARU_EventMask)1 << 63);

/** @brief Bitmask representing all possible event types. */
static const MARU_EventMask MARU_ALL_EVENTS = ~((MARU_EventMask)0);

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* IMPORTANT:
 Every pointer in events at every layer of indirection is to be treated
 as only valid for the duration of the callback.
*/

/** @brief Payload for MARU_WINDOW_READY. */
typedef struct MARU_WindowReadyEvent { char _unused; } MARU_WindowReadyEvent;
/** @brief Payload for MARU_CLOSE_REQUESTED. */
typedef struct MARU_WindowCloseEvent { char _unused; } MARU_WindowCloseEvent;
/** @brief Payload for MARU_WINDOW_FRAME. */
typedef struct MARU_WindowFrameEvent { 
  uint32_t timestamp_ms; 
} MARU_WindowFrameEvent;

/** @brief Payload for MARU_WINDOW_RESIZED. */
typedef struct MARU_WindowResizedEvent {
  MARU_WindowGeometry geometry;
} MARU_WindowResizedEvent;

/** @brief Payload for MARU_WINDOW_MAXIMIZED. */
typedef struct MARU_WindowMaximizationEvent {
  bool maximized;
} MARU_WindowMaximizationEvent;

/** @brief Payload for MARU_KEY_STATE_CHANGED. */
// N.B. This represents raw keyboard data.
// - It does not get fired on keyrepeats
// - the key will likely not match the current keyboard layout
// - If you are using this for text, use MARU_TextInputEvent instead
typedef struct MARU_KeyboardEvent {
  MARU_Key raw_key;
  MARU_ButtonState state;
  MARU_ModifierFlags modifiers;
} MARU_KeyboardEvent;

/** @brief Payload for MARU_MOUSE_MOVED. */
typedef struct MARU_MouseMotionEvent {
  MARU_Vec2Dip position;
  MARU_Vec2Dip delta;
  MARU_Vec2Dip raw_delta;
  MARU_ModifierFlags modifiers;
} MARU_MouseMotionEvent;

/** @brief Payload for MARU_MOUSE_BUTTON_STATE_CHANGED. */
typedef struct MARU_MouseButtonEvent {
  /** Index into maru_getMouseButtonStates()/maru_getMouseButtonChannelInfo(). */
  uint32_t button_id;
  MARU_ButtonState state;
  MARU_ModifierFlags modifiers;
} MARU_MouseButtonEvent;

/** @brief Payload for MARU_MOUSE_SCROLLED. */
typedef struct MARU_MouseScrollEvent {
  MARU_Vec2Dip delta;
  struct { int32_t x; int32_t y; } steps;
  MARU_ModifierFlags modifiers;
} MARU_MouseScrollEvent;

/** @brief Payload for MARU_IDLE_STATE_CHANGED. */
typedef struct MARU_IdleEvent {
  uint32_t timeout_ms;
  bool is_idle;
} MARU_IdleEvent;

/** @brief Payload for MARU_MONITOR_CONNECTION_CHANGED. */
typedef struct MARU_MonitorConnectionEvent {
  MARU_Monitor *monitor;
  bool connected;
} MARU_MonitorConnectionEvent;

/** @brief Payload for MARU_MONITOR_MODE_CHANGED. */
typedef struct MARU_MonitorModeEvent {
  MARU_Monitor *monitor;
} MARU_MonitorModeEvent;

/** @brief Payload for MARU_TEXT_INPUT_RECEIVED. */
typedef struct MARU_TextInputEvent {
  const char *text;
  uint32_t length;
} MARU_TextInputEvent;

/** @brief Payload for MARU_TEXT_COMPOSITION_UPDATED. */
typedef struct MARU_TextCompositionEvent {
  const char *text;
  uint32_t length;
  uint32_t cursor_index;
  uint32_t selection_length;
} MARU_TextCompositionEvent;

/** @brief Payload for MARU_FOCUS_CHANGED. */
typedef struct MARU_FocusEvent {
  bool focused;
} MARU_FocusEvent;

/** @brief UTF-8 byte range. */
typedef struct MARU_TextRangeUtf8 {
  uint32_t start_byte;
  uint32_t length_byte;
} MARU_TextRangeUtf8;

/** @brief Payload for MARU_TEXT_EDIT_START. */
typedef struct MARU_TextEditStartEvent {
  uint64_t session_id;
} MARU_TextEditStartEvent;

/** @brief Payload for MARU_TEXT_EDIT_UPDATE. */
typedef struct MARU_TextEditUpdateEvent {
  uint64_t session_id;
  const char *preedit_utf8;
  uint32_t preedit_length;
  MARU_TextRangeUtf8 caret;
  MARU_TextRangeUtf8 selection;
} MARU_TextEditUpdateEvent;

/** @brief Payload for MARU_TEXT_EDIT_COMMIT. */
typedef struct MARU_TextEditCommitEvent {
  uint64_t session_id;
  uint32_t delete_before_bytes;
  uint32_t delete_after_bytes;
  const char *committed_utf8;
  uint32_t committed_length;
} MARU_TextEditCommitEvent;

/** @brief Payload for MARU_TEXT_EDIT_END. */
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
    MARU_WindowMaximizationEvent maximized;
    MARU_KeyboardEvent key;
    MARU_MouseMotionEvent mouse_motion;
    MARU_MouseButtonEvent mouse_button;
    MARU_MouseScrollEvent mouse_scroll;
    MARU_IdleEvent idle;
    MARU_MonitorConnectionEvent monitor_connection;
    MARU_MonitorModeEvent monitor_mode;
    MARU_WindowFrameEvent frame;
    MARU_TextInputEvent text_input;
    MARU_TextCompositionEvent text_composition;
    MARU_FocusEvent focus;
    MARU_TextEditStartEvent text_edit_start;
    MARU_TextEditUpdateEvent text_edit_update;
    MARU_TextEditCommitEvent text_edit_commit;
    MARU_TextEditEndEvent text_edit_end;

    MARU_UserDefinedEvent user;
  };
} MARU_Event;

/** @brief Callback signature for event dispatching. */
typedef void (*MARU_EventCallback)(MARU_EventType type, MARU_Window *window, const struct MARU_Event *evt, void *userdata);

/** @brief Synchronizes the event queue with the OS and dispatches events to the provided callback. */
MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata);

/** @brief Manually pushes a user-defined event into the queue.
 *
 *  This is a GLOBALLY THREAD-SAFE function and can be called from any thread 
 *  without external synchronization. The passed event will be COPIED.
 */
MARU_Status maru_postEvent(MARU_Context *context, MARU_EventType type,
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
