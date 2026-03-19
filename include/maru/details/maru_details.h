// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU__INCLUDING_DETAILS_FROM_MARU_H
#error "Do not include maru/details/maru_details.h directly; include maru/maru.h."
#endif

#ifndef MARU_DETAILS_H_INCLUDED
#define MARU_DETAILS_H_INCLUDED

#include "maru/maru.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(MARU_VALIDATE_API_CALLS)
#include <stdio.h>
#include <stdlib.h>
#define MARU_VALIDATE_API(cond)                                                \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "Maru API contract violation in %s\n", __func__);        \
      abort();                                                                 \
    }                                                                          \
  } while (0)
#else
#define MARU_VALIDATE_API(cond) (void)0
#endif

#if defined(MARU_ENABLE_INTERNAL_CHECKS)
#include <stdio.h>
#include <stdlib.h>
#define MARU_ASSUME(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      fprintf(stderr, "Maru internal assumption failure in %s\n", __func__);   \
      abort();                                                                 \
    }                                                                          \
  } while (0)
#else
#if defined(__clang__)
#define MARU_ASSUME(cond) __builtin_assume(cond)
#elif defined(_MSC_VER)
#define MARU_ASSUME(cond) __assume(cond)
#elif defined(__GNUC__)
#define MARU_ASSUME(cond) ((cond) ? (void)0 : __builtin_unreachable())
#else
#define MARU_ASSUME(cond) (void)0
#endif
#endif

#define MARU_CONTEXT_STATE_LOST MARU_BIT(0)

#define MARU_MONITOR_STATE_LOST MARU_BIT(0)

#define MARU_WINDOW_STATE_READY MARU_BIT(1)
#define MARU_WINDOW_STATE_FOCUSED MARU_BIT(2)
#define MARU_WINDOW_STATE_MAXIMIZED MARU_BIT(3)
#define MARU_WINDOW_STATE_FULLSCREEN MARU_BIT(4)
#define MARU_WINDOW_STATE_RESIZABLE MARU_BIT(5)
#define MARU_WINDOW_STATE_DECORATED MARU_BIT(6)
#define MARU_WINDOW_STATE_VISIBLE MARU_BIT(7)
#define MARU_WINDOW_STATE_MINIMIZED MARU_BIT(8)

#define MARU_CONTROLLER_STATE_LOST MARU_BIT(0)

/*
 * This file is not part of Maru's source-level API.
 * Application code must not include it directly, cast opaque handles to these
 * types, or depend on their field names.
 *
 * However, maru.h exposes public inline accessors that rely on the leading
 * layout of some handle storage declared here. Those leading layouts are
 * therefore part of Maru's ABI contract by design.
 */
typedef struct MARU_ContextPrefix {
  void* userdata;
  uint64_t flags;
  MARU_BackendType backend_type;
  const MARU_ButtonState8* mouse_button_state;
  const MARU_ChannelInfo* mouse_button_channels;
  uint32_t mouse_button_count;
  const MARU_ButtonState8* keyboard_state;
  uint32_t keyboard_key_count;
} MARU_ContextPrefix;

typedef struct MARU_ImagePrefix {
  void* userdata;
} MARU_ImagePrefix;

typedef struct MARU_CursorPrefix {
  void* userdata;
  uint64_t flags;
} MARU_CursorPrefix;

typedef struct MARU_MonitorPrefix {
  void* userdata;
  MARU_Context* context;
  uint64_t flags;
  const char* name;
  MARU_Vec2Mm physical_size;
  MARU_VideoMode current_mode;
  MARU_Vec2Dip dip_position;
  MARU_Vec2Dip dip_size;
  bool is_primary;
  MARU_Scalar scale;
} MARU_MonitorPrefix;

typedef struct MARU_WindowPrefix {
  void* userdata;
  MARU_Context* context;
  MARU_WindowId window_id;
  uint64_t flags;
  MARU_CursorMode cursor_mode;
  const MARU_Cursor* current_cursor;
  const char* title;
  const MARU_Image* icon;
  MARU_WindowGeometry geometry;
} MARU_WindowPrefix;

typedef struct MARU_ControllerPrefix {
  void* userdata;
  MARU_Context* context;
  uint64_t flags;
  const char* name;
  uint16_t vendor_id;
  uint16_t product_id;
  uint16_t version;
  MARU_Guid guid;
  bool is_standardized;
  const MARU_ChannelInfo* analog_channels;
  const MARU_AnalogInputState* analogs;
  uint32_t analog_count;
  const MARU_ChannelInfo* button_channels;
  const MARU_ButtonState8* buttons;
  uint32_t button_count;
  const MARU_ChannelInfo* haptic_channels;
  uint32_t haptic_count;
} MARU_ControllerPrefix;

typedef struct MARU_DropSessionPrefix {
  MARU_DropAction* action;
  const MARU_DropActionMask* available_actions;
  void** session_userdata;
} MARU_DropSessionPrefix;

static inline MARU_BackendType maru_getContextBackend(const MARU_Context* context) {
  MARU_VALIDATE_API(context != NULL);
  return ((const MARU_ContextPrefix*)context)->backend_type;
}

static inline void* maru_getContextUserdata(const MARU_Context* context) {
  MARU_VALIDATE_API(context != NULL);
  return ((const MARU_ContextPrefix*)context)->userdata;
}

static inline void maru_setContextUserdata(MARU_Context* context, void* userdata) {
  MARU_VALIDATE_API(context != NULL);
  ((MARU_ContextPrefix*)context)->userdata = userdata;
}

static inline bool maru_isContextLost(const MARU_Context* context) {
  MARU_VALIDATE_API(context != NULL);
  return (((const MARU_ContextPrefix*)context)->flags & MARU_CONTEXT_STATE_LOST) != 0;
}

static inline uint32_t maru_getMouseButtonCount(const MARU_Context* context) {
  MARU_VALIDATE_API(context != NULL);
  return ((const MARU_ContextPrefix*)context)->mouse_button_count;
}

static inline const MARU_ButtonState8* maru_getMouseButtonStates(
    const MARU_Context* context) {
  MARU_VALIDATE_API(context != NULL);
  return ((const MARU_ContextPrefix*)context)->mouse_button_state;
}

static inline const MARU_ChannelInfo* maru_getMouseButtonChannelInfo(
    const MARU_Context* context) {
  MARU_VALIDATE_API(context != NULL);
  return ((const MARU_ContextPrefix*)context)->mouse_button_channels;
}

static inline const MARU_ButtonState8* maru_getKeyboardKeyStates(const MARU_Context* context) {
  MARU_VALIDATE_API(context != NULL);
  return ((const MARU_ContextPrefix*)context)->keyboard_state;
}

static inline bool maru_isMouseButtonPressed(const MARU_Context* context,
                                             uint32_t button_id) {
  MARU_VALIDATE_API(context != NULL);
  uint32_t count = maru_getMouseButtonCount(context);
  MARU_VALIDATE_API(button_id < count);
  const MARU_ButtonState8* states = maru_getMouseButtonStates(context);
  return states ? (states[button_id] == MARU_BUTTON_STATE_PRESSED) : false;
}

static inline void* maru_getImageUserdata(const MARU_Image* image) {
  MARU_VALIDATE_API(image != NULL);
  return ((const MARU_ImagePrefix*)image)->userdata;
}

static inline void maru_setImageUserdata(MARU_Image* image, void* userdata) {
  MARU_VALIDATE_API(image != NULL);
  ((MARU_ImagePrefix*)image)->userdata = userdata;
}

static inline void* maru_getCursorUserdata(const MARU_Cursor* cursor) {
  MARU_VALIDATE_API(cursor != NULL);
  return ((const MARU_CursorPrefix*)cursor)->userdata;
}

static inline void maru_setCursorUserdata(MARU_Cursor* cursor, void* userdata) {
  MARU_VALIDATE_API(cursor != NULL);
  ((MARU_CursorPrefix*)cursor)->userdata = userdata;
}

static inline bool maru_isCursorSystem(const MARU_Cursor* cursor) {
  MARU_VALIDATE_API(cursor != NULL);
  return (((const MARU_CursorPrefix*)cursor)->flags & MARU_CURSOR_FLAG_SYSTEM) != 0;
}

static inline void* maru_getMonitorUserdata(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  return ((const MARU_MonitorPrefix*)monitor)->userdata;
}

static inline void maru_setMonitorUserdata(MARU_Monitor* monitor, void* userdata) {
  MARU_VALIDATE_API(monitor != NULL);
  ((MARU_MonitorPrefix*)monitor)->userdata = userdata;
}

static inline const MARU_Context* maru_getMonitorContext(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  const MARU_Context* context = ((const MARU_MonitorPrefix*)monitor)->context;
  MARU_ASSUME(context != NULL);
  return context;
}

static inline bool maru_isMonitorLost(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  return (((const MARU_MonitorPrefix*)monitor)->flags & MARU_MONITOR_STATE_LOST) != 0;
}

static inline const char* maru_getMonitorName(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  return ((const MARU_MonitorPrefix*)monitor)->name;
}

static inline MARU_Vec2Mm maru_getMonitorPhysicalSize(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  return ((const MARU_MonitorPrefix*)monitor)->physical_size;
}

static inline MARU_VideoMode maru_getMonitorCurrentMode(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  return ((const MARU_MonitorPrefix*)monitor)->current_mode;
}

static inline MARU_Vec2Dip maru_getMonitorDipPosition(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  return ((const MARU_MonitorPrefix*)monitor)->dip_position;
}

static inline MARU_Vec2Dip maru_getMonitorDipSize(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  return ((const MARU_MonitorPrefix*)monitor)->dip_size;
}

static inline bool maru_isMonitorPrimary(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  return ((const MARU_MonitorPrefix*)monitor)->is_primary;
}

static inline MARU_Scalar maru_getMonitorScale(const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(monitor != NULL);
  return ((const MARU_MonitorPrefix*)monitor)->scale;
}

static inline void* maru_getWindowUserdata(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return ((const MARU_WindowPrefix*)window)->userdata;
}

static inline void maru_setWindowUserdata(MARU_Window* window, void* userdata) {
  MARU_VALIDATE_API(window != NULL);
  ((MARU_WindowPrefix*)window)->userdata = userdata;
}

static inline const MARU_Context* maru_getWindowContext(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  const MARU_Context* context = ((const MARU_WindowPrefix*)window)->context;
  MARU_ASSUME(context != NULL);
  return context;
}

static inline MARU_WindowId maru_getWindowId(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return ((const MARU_WindowPrefix*)window)->window_id;
}

static inline const char* maru_getWindowTitle(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return ((const MARU_WindowPrefix*)window)->title;
}

static inline bool maru_isWindowReady(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_READY) != 0;
}

static inline bool maru_isWindowFocused(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_FOCUSED) != 0;
}

static inline bool maru_isWindowMaximized(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
}

static inline bool maru_isWindowFullscreen(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_FULLSCREEN) != 0;
}

static inline bool maru_isWindowVisible(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_VISIBLE) != 0;
}

static inline bool maru_isWindowMinimized(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
}

static inline bool maru_isWindowResizable(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
}

static inline bool maru_isWindowDecorated(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return (((const MARU_WindowPrefix*)window)->flags & MARU_WINDOW_STATE_DECORATED) != 0;
}

static inline MARU_CursorMode maru_getWindowCursorMode(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return ((const MARU_WindowPrefix*)window)->cursor_mode;
}

static inline const MARU_Image* maru_getWindowIcon(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return ((const MARU_WindowPrefix*)window)->icon;
}

static inline MARU_WindowGeometry maru_getWindowGeometry(const MARU_Window* window) {
  MARU_VALIDATE_API(window != NULL);
  return ((const MARU_WindowPrefix*)window)->geometry;
}

static inline void* maru_getControllerUserdata(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->userdata;
}

static inline void maru_setControllerUserdata(MARU_Controller* controller, void* userdata) {
  MARU_VALIDATE_API(controller != NULL);
  ((MARU_ControllerPrefix*)controller)->userdata = userdata;
}

static inline const MARU_Context* maru_getControllerContext(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  const MARU_Context* context = ((const MARU_ControllerPrefix*)controller)->context;
  MARU_ASSUME(context != NULL);
  return context;
}

static inline bool maru_isControllerLost(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return (((const MARU_ControllerPrefix*)controller)->flags & MARU_CONTROLLER_STATE_LOST) != 0;
}

static inline const char* maru_getControllerName(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->name;
}

static inline uint16_t maru_getControllerVendorId(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->vendor_id;
}

static inline uint16_t maru_getControllerProductId(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->product_id;
}

static inline uint16_t maru_getControllerVersion(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->version;
}

static inline MARU_Guid maru_getControllerGuid(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->guid;
}

static inline bool maru_isControllerStandardized(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->is_standardized;
}

static inline uint32_t maru_getControllerAnalogCount(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->analog_count;
}

static inline const MARU_ChannelInfo* maru_getControllerAnalogChannelInfo(
    const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->analog_channels;
}

static inline const MARU_AnalogInputState* maru_getControllerAnalogStates(
    const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->analogs;
}

static inline uint32_t maru_getControllerButtonCount(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->button_count;
}

static inline const MARU_ChannelInfo* maru_getControllerButtonChannelInfo(
    const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->button_channels;
}

static inline const MARU_ButtonState8* maru_getControllerButtonStates(
    const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->buttons;
}

static inline bool maru_isControllerButtonPressed(const MARU_Controller* controller,
                                                  uint32_t button_id) {
  MARU_VALIDATE_API(controller != NULL);
  MARU_VALIDATE_API(button_id < maru_getControllerButtonCount(controller));
  const MARU_ButtonState8* states = maru_getControllerButtonStates(controller);
  return states ? (states[button_id] == MARU_BUTTON_STATE_PRESSED) : false;
}

static inline uint32_t maru_getControllerHapticCount(const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->haptic_count;
}

static inline const MARU_ChannelInfo* maru_getControllerHapticChannelInfo(
    const MARU_Controller* controller) {
  MARU_VALIDATE_API(controller != NULL);
  return ((const MARU_ControllerPrefix*)controller)->haptic_channels;
}

static inline void maru_setDropSessionAction(MARU_DropSession* session, MARU_DropAction action) {
  MARU_VALIDATE_API(session != NULL);
  const MARU_DropSessionPrefix* prefix = (const MARU_DropSessionPrefix*)session;
  MARU_VALIDATE_API(prefix->action != NULL);
  MARU_VALIDATE_API(prefix->available_actions != NULL);
  MARU_VALIDATE_API(action == MARU_DROP_ACTION_NONE || action == MARU_DROP_ACTION_COPY ||
              action == MARU_DROP_ACTION_MOVE || action == MARU_DROP_ACTION_LINK);
  MARU_VALIDATE_API(action == MARU_DROP_ACTION_NONE ||
              (((MARU_DropActionMask)action &
                *prefix->available_actions) == (MARU_DropActionMask)action));
  *(prefix->action) = action;
}

static inline MARU_DropAction maru_getDropSessionAction(const MARU_DropSession* session) {
  MARU_VALIDATE_API(session != NULL);
  return *(((const MARU_DropSessionPrefix*)session)->action);
}

static inline void maru_setDropSessionUserdata(MARU_DropSession* session, void* userdata) {
  MARU_VALIDATE_API(session != NULL);
  *(((MARU_DropSessionPrefix*)session)->session_userdata) = userdata;
}

static inline void* maru_getDropSessionUserdata(const MARU_DropSession* session) {
  MARU_VALIDATE_API(session != NULL);
  return *(((const MARU_DropSessionPrefix*)session)->session_userdata);
}

static inline bool maru_isKeyboardKeyPressed(const MARU_Context* context, MARU_Key key) {
  MARU_VALIDATE_API(context != NULL);
  MARU_VALIDATE_API((uint32_t)key < MARU_KEY_COUNT);
  const MARU_ButtonState8* states = maru_getKeyboardKeyStates(context);
  return states ? (states[key] == MARU_BUTTON_STATE_PRESSED) : false;
}

static inline MARU_Status maru_setContextInhibitsSystemIdle(MARU_Context* context, bool enabled) {
  MARU_VALIDATE_API(context != NULL);
  MARU_ContextAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.inhibit_idle = enabled;
  return maru_updateContext(context, MARU_CONTEXT_ATTR_INHIBIT_IDLE, &attrs);
}


static inline MARU_Status maru_setContextIdleTimeout(MARU_Context* context, uint32_t timeout_ms) {
  MARU_VALIDATE_API(context != NULL);
  MARU_ContextAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.idle_timeout_ms = timeout_ms;
  return maru_updateContext(context, MARU_CONTEXT_ATTR_IDLE_TIMEOUT, &attrs);
}

static inline MARU_Status maru_setContextDiagnosticCallback(MARU_Context* context, MARU_DiagnosticCallback cb, void* userdata) {
  MARU_VALIDATE_API(context != NULL);
  MARU_ContextAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.diagnostic_cb = cb;
  attrs.diagnostic_userdata = userdata;
  return maru_updateContext(context, MARU_CONTEXT_ATTR_DIAGNOSTICS, &attrs);
}

static inline MARU_Status maru_setWindowDipViewportSize(MARU_Window* window, MARU_Vec2Dip size) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.dip_viewport_size = size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowSurroundingText(MARU_Window* window, const char* text, uint32_t cursor_byte, uint32_t anchor_byte) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.surrounding_text = text;
  attrs.surrounding_cursor_byte = cursor_byte;
  attrs.surrounding_anchor_byte = anchor_byte;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_SURROUNDING_TEXT | MARU_WINDOW_ATTR_SURROUNDING_CURSOR_BYTE | MARU_WINDOW_ATTR_SURROUNDING_ANCHOR_BYTE, &attrs);
}

static inline MARU_Status maru_setWindowTitle(MARU_Window* window, const char* title) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.title = title;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_TITLE, &attrs);
}

static inline MARU_Status maru_setWindowDipSize(MARU_Window* window, MARU_Vec2Dip size) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.dip_size = size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_DIP_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowDipPosition(MARU_Window* window, MARU_Vec2Dip position) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.dip_position = position;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_DIP_POSITION, &attrs);
}

static inline bool
_maru_window_attrs_is_fullscreen(const MARU_WindowAttributes* attrs) {
  MARU_VALIDATE_API(attrs != NULL);
  return attrs->presentation_state == MARU_WINDOW_PRESENTATION_FULLSCREEN;
}

static inline bool
_maru_window_attrs_is_maximized(const MARU_WindowAttributes* attrs) {
  MARU_VALIDATE_API(attrs != NULL);
  return attrs->presentation_state == MARU_WINDOW_PRESENTATION_MAXIMIZED;
}

static inline bool
_maru_window_attrs_is_minimized(const MARU_WindowAttributes* attrs) {
  MARU_VALIDATE_API(attrs != NULL);
  return attrs->presentation_state == MARU_WINDOW_PRESENTATION_MINIMIZED;
}

static inline void _maru_window_attributes_set_presentation_state(
    MARU_WindowAttributes* attrs, MARU_WindowPresentationState state) {
  MARU_VALIDATE_API(attrs != NULL);
  attrs->presentation_state = state;
}

static inline MARU_Status maru_setWindowFullscreen(MARU_Window* window, bool enabled) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  _maru_window_attributes_set_presentation_state(
      &attrs,
      enabled ? MARU_WINDOW_PRESENTATION_FULLSCREEN : MARU_WINDOW_PRESENTATION_NORMAL);
  attrs.visible = true;
  const uint64_t mask = MARU_WINDOW_ATTR_PRESENTATION_STATE | MARU_WINDOW_ATTR_VISIBLE;
  return maru_updateWindow(window, mask, &attrs);
}

static inline MARU_Status maru_setWindowMaximized(MARU_Window* window, bool enabled) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  _maru_window_attributes_set_presentation_state(
      &attrs,
      enabled ? MARU_WINDOW_PRESENTATION_MAXIMIZED : MARU_WINDOW_PRESENTATION_NORMAL);
  attrs.visible = true;
  const uint64_t mask = MARU_WINDOW_ATTR_PRESENTATION_STATE | MARU_WINDOW_ATTR_VISIBLE;
  return maru_updateWindow(window, mask, &attrs);
}

static inline MARU_Status maru_setWindowCursorMode(MARU_Window* window, MARU_CursorMode mode) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.cursor_mode = mode;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR_MODE, &attrs);
}

static inline MARU_Status maru_setWindowCursor(MARU_Window* window, const MARU_Cursor* cursor) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.cursor = cursor;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR, &attrs);
}

static inline MARU_Status maru_setWindowMonitor(MARU_Window* window, const MARU_Monitor* monitor) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.monitor = monitor;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MONITOR, &attrs);
}

static inline MARU_Status maru_setWindowDipMinSize(MARU_Window* window, MARU_Vec2Dip dip_min_size) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.dip_min_size = dip_min_size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_DIP_MIN_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowDipMaxSize(MARU_Window* window, MARU_Vec2Dip dip_max_size) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.dip_max_size = dip_max_size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_DIP_MAX_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowAspectRatio(MARU_Window* window,
                                                    MARU_Fraction aspect_ratio) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.aspect_ratio = aspect_ratio;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_ASPECT_RATIO, &attrs);
}

static inline MARU_Status maru_setWindowResizable(MARU_Window* window, bool enabled) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.resizable = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_RESIZABLE, &attrs);
}

static inline MARU_Status maru_setWindowTextInputType(MARU_Window* window,
                                                      MARU_TextInputType type) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.text_input_type = type;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_TEXT_INPUT_TYPE, &attrs);
}

static inline MARU_Status maru_setWindowDipTextInputRect(MARU_Window* window, MARU_RectDip rect) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.dip_text_input_rect = rect;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT, &attrs);
}

static inline MARU_Status maru_setWindowAcceptDrop(MARU_Window* window, bool enabled) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.accept_drop = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_ACCEPT_DROP, &attrs);
}

static inline MARU_Status maru_setWindowVisible(MARU_Window* window, bool visible) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.visible = visible;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_VISIBLE, &attrs);
}

static inline MARU_Status maru_setWindowMinimized(MARU_Window* window, bool minimized) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  _maru_window_attributes_set_presentation_state(
      &attrs,
      minimized ? MARU_WINDOW_PRESENTATION_MINIMIZED : MARU_WINDOW_PRESENTATION_NORMAL);
  attrs.visible = !minimized;
  const uint64_t mask = MARU_WINDOW_ATTR_PRESENTATION_STATE | MARU_WINDOW_ATTR_VISIBLE;
  return maru_updateWindow(window, mask, &attrs);
}

static inline MARU_Status maru_setWindowIcon(MARU_Window* window, const MARU_Image* icon) {
  MARU_VALIDATE_API(window != NULL);
  MARU_WindowAttributes attrs;
  memset(&attrs, 0, sizeof(attrs));
  attrs.icon = icon;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_ICON, &attrs);
}

#ifdef __cplusplus
}
#endif

#endif
