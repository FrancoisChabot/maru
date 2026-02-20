/**
 * @note THIS API HAS BEEN SUPERSEDED AND IS KEPT FOR REFERENCE ONLY.
 */
// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CONVENIENCE_IMPLS_H_INCLUDED
#define MARU_CONVENIENCE_IMPLS_H_INCLUDED

#include "maru/c/convenience.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline MARU_Status maru_setContextInhibitsSystemIdle(MARU_Context *context, bool enabled) {
  MARU_ContextAttributes attrs = {0};
  attrs.inhibit_idle = enabled;
  return maru_updateContext(context, MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE, &attrs);
}

static inline MARU_Status maru_setWindowTitle(MARU_Window *window, const char *title) {
  MARU_WindowAttributes attrs = {0};
  attrs.title = title;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_TITLE, &attrs);
}

static inline MARU_Status maru_setWindowSize(MARU_Window *window, MARU_Vec2Dip size) {
  MARU_WindowAttributes attrs = {0};
  attrs.logical_size = size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_LOGICAL_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowFullscreen(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs = {0};
  attrs.fullscreen = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_FULLSCREEN, &attrs);
}

static inline MARU_Status maru_setWindowMaximized(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs = {0};
  attrs.maximized = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MAXIMIZED, &attrs);
}

static inline MARU_Status maru_setWindowCursorMode(MARU_Window *window, MARU_CursorMode mode) {
  MARU_WindowAttributes attrs = {0};
  attrs.cursor_mode = mode;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR_MODE, &attrs);
}

static inline MARU_Status maru_setWindowCursor(MARU_Window *window, MARU_Cursor *cursor) {
  MARU_WindowAttributes attrs = {0};
  attrs.cursor = cursor;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR, &attrs);
}

static inline MARU_Status maru_setWindowMonitor(MARU_Window *window, MARU_Monitor *monitor) {
  MARU_WindowAttributes attrs = {0};
  attrs.monitor = monitor;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MONITOR, &attrs);
}

static inline MARU_Status maru_setWindowMinSize(MARU_Window *window, MARU_Vec2Dip min_size) {
  MARU_WindowAttributes attrs = {0};
  attrs.min_size = min_size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MIN_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowMaxSize(MARU_Window *window, MARU_Vec2Dip max_size) {
  MARU_WindowAttributes attrs = {0};
  attrs.max_size = max_size;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MAX_SIZE, &attrs);
}

static inline MARU_Status maru_setWindowAspectRatio(MARU_Window *window, MARU_Fraction aspect_ratio) {
  MARU_WindowAttributes attrs = {0};
  attrs.aspect_ratio = aspect_ratio;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_ASPECT_RATIO, &attrs);
}

static inline MARU_Status maru_setWindowResizable(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs = {0};
  attrs.resizable = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_RESIZABLE, &attrs);
}

static inline MARU_Status maru_setWindowDecorated(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs = {0};
  attrs.decorated = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_DECORATED, &attrs);
}

static inline MARU_Status maru_setWindowMousePassthrough(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs = {0};
  attrs.mouse_passthrough = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH, &attrs);
}

static inline MARU_Status maru_setWindowTextInputType(MARU_Window *window, MARU_TextInputType type) {
  MARU_WindowAttributes attrs = {0};
  attrs.text_input_type = type;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_TEXT_INPUT_TYPE, &attrs);
}

static inline MARU_Status maru_setWindowTextInputRect(MARU_Window *window, MARU_RectDip rect) {
  MARU_WindowAttributes attrs = {0};
  attrs.text_input_rect = rect;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_TEXT_INPUT_RECT, &attrs);
}

static inline MARU_Status maru_setWindowAcceptDrop(MARU_Window *window, bool enabled) {
  MARU_WindowAttributes attrs = {0};
  attrs.accept_drop = enabled;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_ACCEPT_DROP, &attrs);
}

static inline MARU_Status maru_setWindowEventMask(MARU_Window *window, MARU_EventMask mask) {
  MARU_WindowAttributes attrs = {0};
  attrs.event_mask = mask;
  return maru_updateWindow(window, MARU_WINDOW_ATTR_EVENT_MASK, &attrs);
}

static inline MARU_Status maru_requestText(MARU_Window *window,
                                           MARU_DataExchangeTarget target,
                                           void *user_tag) {
  return maru_requestData(window, target, "text/plain; charset=utf-8", user_tag);
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
