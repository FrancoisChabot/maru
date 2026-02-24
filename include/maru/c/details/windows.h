// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DETAILS_WINDOWS_H_INCLUDED
#define MARU_DETAILS_WINDOWS_H_INCLUDED

#include "maru/c/core.h"
#include "maru/c/metrics.h"
#include "maru/c/windows.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;
typedef struct MARU_Cursor MARU_Cursor;

/** @brief Internal representation of MARU_Window. 
    CHANGING THIS REQUIRES A MAJOR VERSION BUMP
*/
typedef struct MARU_WindowExposed {
  void *userdata;
  MARU_Context *context;
  uint64_t flags;
  const MARU_WindowMetrics *metrics;

  const MARU_ButtonState8 *keyboard_state;
  uint32_t keyboard_key_count;
  const MARU_ButtonState8 *mouse_button_state;
  const MARU_MouseButtonChannelInfo *mouse_button_channels;
  uint32_t mouse_button_count;
  int32_t mouse_default_button_channels[MARU_MOUSE_DEFAULT_COUNT];
  MARU_EventMask event_mask;
  MARU_CursorMode cursor_mode;
  const MARU_Cursor *current_cursor;
  const char *title;
} MARU_WindowExposed;

static inline void *maru_getWindowUserdata(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->userdata;
}

static inline void maru_setWindowUserdata(MARU_Window *window, void *userdata) {
  ((MARU_WindowExposed *)window)->userdata = userdata;
}

static inline MARU_Context *maru_getWindowContext(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->context;
}

static inline const char *maru_getWindowTitle(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->title;
}

static inline bool maru_isWindowLost(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_LOST) != 0;
}

static inline bool maru_isWindowReady(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_READY) != 0;
}

static inline bool maru_isWindowFocused(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_FOCUSED) != 0;
}

static inline bool maru_isWindowMaximized(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_MAXIMIZED) != 0;
}

static inline bool maru_isWindowFullscreen(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_FULLSCREEN) != 0;
}

static inline bool maru_isWindowMousePassthrough(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_MOUSE_PASSTHROUGH) != 0;
}

static inline bool maru_isWindowVisible(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_VISIBLE) != 0;
}

static inline bool maru_isWindowMinimized(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_MINIMIZED) != 0;
}

static inline bool maru_isWindowResizable(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_RESIZABLE) != 0;
}

static inline bool maru_isWindowDecorated(const MARU_Window *window) {
  return (((const MARU_WindowExposed *)window)->flags & MARU_WINDOW_STATE_DECORATED) != 0;
}

static inline MARU_EventMask maru_getWindowEventMask(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->event_mask;
}

static inline const MARU_WindowMetrics *maru_getWindowMetrics(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->metrics;
}

static inline MARU_CursorMode maru_getWindowCursorMode(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->cursor_mode;
}

#ifdef __cplusplus
}
#endif

#endif  // MARU_DETAILS_WINDOWS_H_INCLUDED
