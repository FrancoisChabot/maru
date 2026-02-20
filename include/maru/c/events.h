// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_EVENTS_H_INCLUDED
#define MARU_EVENTS_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/geometry.h"
#include "maru/c/inputs.h"
#include "maru/c/data_exchange.h"
#ifdef MARU_ENABLE_CONTROLLERS
#include "maru/c/ext/controllers.h"
#endif

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
/** @brief Opaque handle representing an active controller connection. */
typedef struct MARU_Controller MARU_Controller;


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
/** @brief Event fired when a requested synchronization point is reached. */
static const MARU_EventMask MARU_SYNC_POINT_REACHED = ((MARU_EventMask)1 << 10);
/** @brief Event fired when Unicode text input is received. */
static const MARU_EventMask MARU_TEXT_INPUT_RECEIVED = ((MARU_EventMask)1 << 11);
/** @brief Event fired when a window gains or loses focus. */
static const MARU_EventMask MARU_FOCUS_CHANGED = ((MARU_EventMask)1 << 12);
/** @brief Event fired when a window is maximized or restored. */
static const MARU_EventMask MARU_WINDOW_MAXIMIZED = ((MARU_EventMask)1 << 13);
/** @brief Event fired during a drag-and-drop operation when first entering the window. */
static const MARU_EventMask MARU_DROP_ENTERED = ((MARU_EventMask)1 << 14);
/** @brief Event fired during a drag-and-drop operation when hovering. */
static const MARU_EventMask MARU_DROP_HOVERED = ((MARU_EventMask)1 << 15);
/** @brief Event fired when a drag-and-drop operation leaves a window. */
static const MARU_EventMask MARU_DROP_EXITED = ((MARU_EventMask)1 << 16);
/** @brief Event fired when a drag-and-drop payload is dropped on a window. */
static const MARU_EventMask MARU_DROP_DROPPED = ((MARU_EventMask)1 << 17);
/** @brief Event fired when IME text composition state changes. */
static const MARU_EventMask MARU_TEXT_COMPOSITION_UPDATED = ((MARU_EventMask)1 << 18);
/** @brief Event fired when asynchronous data has been received. */
static const MARU_EventMask MARU_DATA_RECEIVED = ((MARU_EventMask)1 << 19);
/** @brief Event fired when the OS requests data from the application. */
static const MARU_EventMask MARU_DATA_REQUESTED = ((MARU_EventMask)1 << 20);
/** @brief Event fired when the OS has finished consuming data provided via maru_provideData. */
static const MARU_EventMask MARU_DATA_CONSUMED = ((MARU_EventMask)1 << 21);
/** @brief Event fired on the source window when a drag session completes. */
static const MARU_EventMask MARU_DRAG_FINISHED = ((MARU_EventMask)1 << 22);

  #ifdef MARU_ENABLE_CONTROLLERS
/** @brief Event fired when a game controller is connected or disconnected. */
static const MARU_EventMask MARU_CONTROLLER_CONNECTION_CHANGED = ((MARU_EventMask)1 << 46);
/** @brief Event fired when a controller button state changes. */
static const MARU_EventMask MARU_CONTROLLER_BUTTON_STATE_CHANGED = ((MARU_EventMask)1 << 47);
  #endif

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
/** @brief Payload for MARU_SYNC_POINT_REACHED. */
typedef struct MARU_SyncEvent { char _unused; } MARU_SyncEvent;

/** @brief Payload for MARU_DRAG_FINISHED. */
typedef struct MARU_DragFinishedEvent {
  MARU_DropAction action;
} MARU_DragFinishedEvent;

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
} MARU_MouseMotionEvent;

/** @brief Payload for MARU_MOUSE_BUTTON_STATE_CHANGED. */
typedef struct MARU_MouseButtonEvent {
  MARU_MouseButton button;
  MARU_ButtonState state;
  MARU_ModifierFlags modifiers;
} MARU_MouseButtonEvent;

/** @brief Payload for MARU_MOUSE_SCROLLED. */
typedef struct MARU_MouseScrollEvent {
  MARU_Vec2Dip delta;
  struct { int32_t x; int32_t y; } steps;
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

/** @brief Controls application response to a drag-and-drop hover. */
typedef struct MARU_DropFeedback {
  MARU_DropAction *action;
  void **session_userdata;
} MARU_DropFeedback;

/** @brief Payload for MARU_DROP_ENTERED. */
typedef struct MARU_DropEnterEvent {
  MARU_Vec2Dip position;
  MARU_DropFeedback *feedback;
  const char **paths;   // Can be NULL
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint16_t path_count;
} MARU_DropEnterEvent;


/** @brief Payload for MARU_DROP_HOVERED. */
typedef struct MARU_DropHoverEvent {
  MARU_Vec2Dip position;
  MARU_DropFeedback *feedback;
  const char **paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint16_t path_count;
} MARU_DropHoverEvent;

/** @brief Payload for MARU_DROP_EXITED. */
typedef struct MARU_DropLeaveEvent {
  void **session_userdata;
} MARU_DropLeaveEvent;

/** @brief Payload for MARU_DROP_DROPPED. */
typedef struct MARU_DropEvent {
  MARU_Vec2Dip position;
  void **session_userdata;
  const char **paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint16_t path_count;
} MARU_DropEvent;

/** @brief Payload for MARU_DATA_RECEIVED. */
typedef struct MARU_DataReceivedEvent {
  void *user_tag;
  MARU_Status status;
  MARU_DataExchangeTarget target;
  const char *mime_type;
  const void *data;
  size_t size;
} MARU_DataReceivedEvent;

/** @brief Payload for MARU_DATA_REQUESTED. */
typedef struct MARU_DataRequestEvent {
  MARU_DataExchangeTarget target;
  const char *mime_type;
  void *internal_handle;
} MARU_DataRequestEvent;

/** @brief Payload for MARU_DATA_CONSUMED. */
typedef struct MARU_DataConsumedEvent {
  MARU_DataExchangeTarget target;
  const char *mime_type;
  const void *data;
  size_t size;
  MARU_DropAction action;
} MARU_DataConsumedEvent;

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
    MARU_SyncEvent sync;
    MARU_TextInputEvent text_input;
    MARU_TextCompositionEvent text_composition;
    MARU_FocusEvent focus;
    MARU_DropEnterEvent drop_enter;
    MARU_DropHoverEvent drop_hover;
    MARU_DropLeaveEvent drop_leave;
    MARU_DropEvent drop;
    MARU_DragFinishedEvent drag_finished;
    MARU_DataReceivedEvent data_received;
    MARU_DataRequestEvent data_request;
    MARU_DataConsumedEvent data_consumed;
#ifdef MARU_ENABLE_CONTROLLERS
    MARU_ControllerConnectionEvent controller_connection;
    MARU_ControllerButtonStateChanged controller_button;
#endif

    MARU_UserDefinedEvent user;
  };
} MARU_Event;

/** @brief Synchronizes the event queue with the OS and dispatches events to the registered callback. */
MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms);

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

/** @brief Callback signature for event dispatching. */
typedef void (*MARU_EventCallback)(MARU_EventType type, MARU_Window *window, const struct MARU_Event *evt);

#ifdef __cplusplus
}
#endif

#endif  // MARU_EVENTS_H_INCLUDED
