/**
 * @note THIS API HAS BEEN SUPERSEDED AND IS KEPT FOR REFERENCE ONLY.
 */
// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CONVENIENCE_H_INCLUDED
#define MARU_CONVENIENCE_H_INCLUDED

#include "maru/c/contexts.h"
#include "maru/c/windows.h"
#include "maru/c/monitors.h"
#include "maru/c/cursors.h"
#include "maru/c/data_exchange.h"
#include "maru/c/instrumentation.h"
#ifdef MARU_ENABLE_CONTROLLERS
#include "maru/c/ext/controllers.h"
#endif

/**
 * @file convenience.h
 * @brief Inline convenience functions for common attribute updates.
 */

#ifdef __cplusplus
extern "C" {
#endif

static inline MARU_Status maru_setContextInhibitsSystemIdle(MARU_Context *context, bool enabled);

// N.B. If you are calling a few of those in a row, consider using maru_updateWindow instead.
static inline MARU_Status maru_setWindowTitle(MARU_Window *window, const char *title);
static inline MARU_Status maru_setWindowSize(MARU_Window *window, MARU_Vec2Dip size);
static inline MARU_Status maru_setWindowFullscreen(MARU_Window *window, bool enabled);
static inline MARU_Status maru_setWindowMaximized(MARU_Window *window, bool enabled);
static inline MARU_Status maru_setWindowCursorMode(MARU_Window *window, MARU_CursorMode mode);
static inline MARU_Status maru_setWindowCursor(MARU_Window *window, MARU_Cursor *cursor);
static inline MARU_Status maru_setWindowMonitor(MARU_Window *window, MARU_Monitor *monitor);
static inline MARU_Status maru_setWindowMinSize(MARU_Window *window, MARU_Vec2Dip min_size);
static inline MARU_Status maru_setWindowMaxSize(MARU_Window *window, MARU_Vec2Dip max_size);
static inline MARU_Status maru_setWindowAspectRatio(MARU_Window *window, MARU_Fraction aspect_ratio);
static inline MARU_Status maru_setWindowResizable(MARU_Window *window, bool enabled);
static inline MARU_Status maru_setWindowMousePassthrough(MARU_Window *window, bool enabled);
static inline MARU_Status maru_setWindowTextInputType(MARU_Window *window, MARU_TextInputType type);
static inline MARU_Status maru_setWindowTextInputRect(MARU_Window *window, MARU_RectDip rect);
static inline MARU_Status maru_setWindowAcceptDrop(MARU_Window *window, bool enabled);
static inline MARU_Status maru_setWindowEventMask(MARU_Window *window, MARU_EventMask mask);

static inline MARU_Status maru_requestText(MARU_Window *window, MARU_DataExchangeTarget target,
                                           void *user_tag);

#ifdef MARU_ENABLE_CONTROLLERS
#endif

static inline const MARU_UserEventMetrics *maru_getContextEventMetrics(const MARU_Context *context);

static inline const char *maru_getDiagnosticString(MARU_Diagnostic diagnostic);

#ifdef MARU_ENABLE_CONTROLLERS
#endif

#ifdef __cplusplus
}
#endif

#include "maru/c/details/convenience_impls.h"

#endif  // MARU_CONVENIENCE_H_INCLUDED
