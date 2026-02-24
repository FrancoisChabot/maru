// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CONVENIENCE_IMPLS_H_INCLUDED
#define MARU_CONVENIENCE_IMPLS_H_INCLUDED

#include "maru/c/convenience.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void maru_setContextInhibitsSystemIdle(MARU_Context *context, bool enabled) {
  MARU_ContextAttributes attrs;
  attrs.inhibit_idle = enabled;
  maru_updateContext(context, MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE, &attrs);
}

static inline void maru_setWindowTitle(MARU_Window *window, const char *title) {
  MARU_WindowAttributes attrs;
  attrs.title = title;
  maru_updateWindow(window, MARU_WINDOW_ATTR_TITLE, &attrs);
}

static inline void maru_setWindowSize(MARU_Window *window, MARU_Vec2Dip size) {
  MARU_WindowAttributes attrs;
  attrs.logical_size = size;
  maru_updateWindow(window, MARU_WINDOW_ATTR_LOGICAL_SIZE, &attrs);
}

static inline void maru_setWindowFullscreen(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs;
  attrs.fullscreen = enabled;
  maru_updateWindow(window, MARU_WINDOW_ATTR_FULLSCREEN, &attrs);
}

static inline void maru_setWindowMaximized(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs;
  attrs.maximized = enabled;
  maru_updateWindow(window, MARU_WINDOW_ATTR_MAXIMIZED, &attrs);
}

static inline void maru_setWindowCursorMode(MARU_Window *window, MARU_CursorMode mode) {
  MARU_WindowAttributes attrs;
  attrs.cursor_mode = mode;
  maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR_MODE, &attrs);
}

static inline void maru_setWindowCursor(MARU_Window *window, MARU_Cursor *cursor) {
  MARU_WindowAttributes attrs;
  attrs.cursor = cursor;
  maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR, &attrs);
}

static inline void maru_setWindowMonitor(MARU_Window *window, MARU_Monitor *monitor) {
  MARU_WindowAttributes attrs;
  attrs.monitor = monitor;
  maru_updateWindow(window, MARU_WINDOW_ATTR_MONITOR, &attrs);
}

static inline void maru_setWindowMinSize(MARU_Window *window, MARU_Vec2Dip min_size) {
  MARU_WindowAttributes attrs;
  attrs.min_size = min_size;
  maru_updateWindow(window, MARU_WINDOW_ATTR_MIN_SIZE, &attrs);
}

static inline void maru_setWindowMaxSize(MARU_Window *window, MARU_Vec2Dip max_size) {
  MARU_WindowAttributes attrs;
  attrs.max_size = max_size;
  maru_updateWindow(window, MARU_WINDOW_ATTR_MAX_SIZE, &attrs);
}

static inline void maru_setWindowAspectRatio(MARU_Window *window, MARU_Fraction aspect_ratio) {
  MARU_WindowAttributes attrs;
  attrs.aspect_ratio = aspect_ratio;
  maru_updateWindow(window, MARU_WINDOW_ATTR_ASPECT_RATIO, &attrs);
}

static inline void maru_setWindowResizable(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs;
  attrs.resizable = enabled;
  maru_updateWindow(window, MARU_WINDOW_ATTR_RESIZABLE, &attrs);
}

static inline void maru_setWindowMousePassthrough(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs;
  attrs.mouse_passthrough = enabled;
  maru_updateWindow(window, MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH, &attrs);
}

static inline void maru_setWindowTextInputType(MARU_Window *window, MARU_TextInputType type) {
  MARU_WindowAttributes attrs;
  attrs.text_input_type = type;
  maru_updateWindow(window, MARU_WINDOW_ATTR_TEXT_INPUT_TYPE, &attrs);
}

static inline void maru_setWindowTextInputRect(MARU_Window *window, MARU_RectDip rect) {
  MARU_WindowAttributes attrs;
  attrs.text_input_rect = rect;
  maru_updateWindow(window, MARU_WINDOW_ATTR_TEXT_INPUT_RECT, &attrs);
}

static inline void maru_setWindowEventMask(MARU_Window *window, MARU_EventMask mask) {
  MARU_WindowAttributes attrs;
  attrs.event_mask = mask;
  maru_updateWindow(window, MARU_WINDOW_ATTR_EVENT_MASK, &attrs);
}

static inline void maru_setWindowVisible(MARU_Window *window, bool visible) {
  MARU_WindowAttributes attrs;
  attrs.visible = visible;
  maru_updateWindow(window, MARU_WINDOW_ATTR_VISIBLE, &attrs);
}

static inline void maru_setWindowMinimized(MARU_Window *window, bool minimized) {
  MARU_WindowAttributes attrs;
  attrs.minimized = minimized;
  maru_updateWindow(window, MARU_WINDOW_ATTR_MINIMIZED, &attrs);
}

static inline void maru_setWindowIcon(MARU_Window *window, MARU_Image *icon) {
  MARU_WindowAttributes attrs;
  attrs.icon = icon;
  maru_updateWindow(window, MARU_WINDOW_ATTR_ICON, &attrs);
}

static inline const MARU_UserEventMetrics *maru_getContextEventMetrics(const MARU_Context *context) {
  const MARU_ContextMetrics *metrics = maru_getContextMetrics(context);
  return metrics ? metrics->user_events : NULL;
}

static inline const char *maru_getDiagnosticString(MARU_Diagnostic diagnostic) {
  switch (diagnostic) {
    case MARU_DIAGNOSTIC_NONE: return "NONE";
    case MARU_DIAGNOSTIC_INFO: return "INFO";
    case MARU_DIAGNOSTIC_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
    case MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE: return "RESOURCE_UNAVAILABLE";
    case MARU_DIAGNOSTIC_DYNAMIC_LIB_FAILURE: return "DYNAMIC_LIB_FAILURE";
    case MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED: return "FEATURE_UNSUPPORTED";
    case MARU_DIAGNOSTIC_BACKEND_FAILURE: return "BACKEND_FAILURE";
    case MARU_DIAGNOSTIC_BACKEND_UNAVAILABLE: return "BACKEND_UNAVAILABLE";
    case MARU_DIAGNOSTIC_VULKAN_FAILURE: return "VULKAN_FAILURE";
    case MARU_DIAGNOSTIC_UNKNOWN: return "UNKNOWN";
    case MARU_DIAGNOSTIC_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case MARU_DIAGNOSTIC_PRECONDITION_FAILURE: return "PRECONDITION_FAILURE";
    case MARU_DIAGNOSTIC_INTERNAL: return "INTERNAL";
    default: return "UNKNOWN";
  }
}

#ifdef __cplusplus
}
#endif

#endif  // MARU_CONVENIENCE_IMPLS_H_INCLUDED
