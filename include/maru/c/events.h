// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_EVENTS_H_INCLUDED
#define MARU_EVENTS_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/data_exchange.h"
#include "maru/c/geometry.h"
#include "maru/c/inputs.h"
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

/** @brief Opaque handle representing the library state and display server
 * connection. */
typedef struct MARU_Context MARU_Context;
/** @brief Opaque handle representing an OS-level window. */
typedef struct MARU_Window MARU_Window;
/** @brief Opaque handle representing a physical monitor. */
typedef struct MARU_Monitor MARU_Monitor;
/** @brief Opaque handle representing an active controller connection. */
typedef struct MARU_Controller MARU_Controller;

/** @brief Bitmask for filtering events during polling or waiting. */
typedef uint64_t MARU_EventMask;

/** @brief Represents a single event type */
typedef uint64_t MARU_EventType;

// N.B. static const MARU_EventMask is the best way to represent
// this in a stable manner, but it can trigger noisy warnings.
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-const-variable"
#endif

/** @brief Event fired when a window close request is received. */
static const MARU_EventMask MARU_CLOSE_REQUESTED = ((MARU_EventMask)1 << 0);
static const MARU_EventMask MARU_EVENT_TYPE_CLOSE_REQUESTED =
    ((MARU_EventMask)1 << 0);
/** @brief Event fired when a window has been resized. */
static const MARU_EventMask MARU_WINDOW_RESIZED = ((MARU_EventMask)1 << 1);
static const MARU_EventMask MARU_EVENT_TYPE_WINDOW_RESIZED =
    ((MARU_EventMask)1 << 1);
/** @brief Event fired when a keyboard key state changes. */
static const MARU_EventMask MARU_KEY_STATE_CHANGED = ((MARU_EventMask)1 << 2);
static const MARU_EventMask MARU_EVENT_TYPE_KEY_STATE_CHANGED =
    ((MARU_EventMask)1 << 2);

/** @brief Event fired when a window is fully created and ready for interaction.
 */
static const MARU_EventMask MARU_WINDOW_READY = ((MARU_EventMask)1 << 3);
static const MARU_EventMask MARU_EVENT_TYPE_WINDOW_READY =
    ((MARU_EventMask)1 << 3);

/** @brief Event fired when the mouse cursor moves. */
static const MARU_EventMask MARU_MOUSE_MOVED = ((MARU_EventMask)1 << 4);
static const MARU_EventMask MARU_EVENT_TYPE_MOUSE_MOVED =
    ((MARU_EventMask)1 << 4);

/** @brief Event fired when a mouse button state changes. */
static const MARU_EventMask MARU_MOUSE_BUTTON_STATE_CHANGED =
    ((MARU_EventMask)1 << 5);
static const MARU_EventMask MARU_EVENT_TYPE_MOUSE_BUTTON_STATE_CHANGED =
    ((MARU_EventMask)1 << 5);

/** @brief Event fired when a mouse scroll wheel is moved. */
static const MARU_EventMask MARU_MOUSE_SCROLLED = ((MARU_EventMask)1 << 6);
static const MARU_EventMask MARU_EVENT_TYPE_MOUSE_SCROLLED =
    ((MARU_EventMask)1 << 6);

/** @brief Event fired when Unicode text input is received. */
static const MARU_EventMask MARU_TEXT_INPUT_RECEIVED =
    ((MARU_EventMask)1 << 11);
static const MARU_EventMask MARU_EVENT_TYPE_TEXT_INPUT_RECEIVED =
    ((MARU_EventMask)1 << 11);

/** @brief Bitmask representing all possible event types. */
static const MARU_EventMask MARU_ALL_EVENTS = ~((MARU_EventMask)0);

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

/* IMPORTANT:
 Every pointer in events at every layer of indirection is to be treated
 as only valid for the duration of the callback.
*/

/** @brief Payload for MARU_EVENT_TYPE_WINDOW_READY. */
typedef struct MARU_WindowReadyEvent {
  char _unused;
} MARU_WindowReadyEvent;
/** @brief Payload for MARU_EVENT_TYPE_CLOSE_REQUESTED. */
typedef struct MARU_WindowCloseEvent {
  char _unused;
} MARU_WindowCloseEvent;

/** @brief Payload for MARU_EVENT_TYPE_WINDOW_RESIZED. */
typedef struct MARU_WindowResizedEvent {
  MARU_WindowGeometry geometry;
} MARU_WindowResizedEvent;

/** @brief Payload for MARU_KEY_STATE_CHANGED. */
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
  struct {
    int32_t x;
    int32_t y;
  } steps;
} MARU_MouseScrollEvent;

/** @brief Payload for MARU_TEXT_INPUT_RECEIVED. */
typedef struct MARU_TextInputEvent {
  const char *text;
  uint32_t length;
} MARU_TextInputEvent;

/** @brief Unified standard event payload union. */
typedef struct MARU_Event {
  MARU_EventType type;
  union {
    MARU_WindowReadyEvent window_ready;
    MARU_WindowCloseEvent close_requested;
    MARU_WindowResizedEvent resized;
    MARU_KeyboardEvent key;
    MARU_MouseMotionEvent mouse_motion;
    MARU_MouseButtonEvent mouse_button;
    MARU_MouseScrollEvent mouse_scroll;
    MARU_TextInputEvent text_input;
  };
} MARU_Event;

/** @brief Synchronizes the event queue with the OS and dispatches events to the
 * registered callback. */
MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms);

/** @brief Callback signature for event dispatching. */
typedef void (*MARU_EventCallback)(MARU_EventType type, MARU_Window *window,
                                   const struct MARU_Event *evt);

#ifdef __cplusplus
}
#endif

#endif // MARU_EVENTS_H_INCLUDED
