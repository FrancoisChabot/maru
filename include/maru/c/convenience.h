// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CONVENIENCE_H_INCLUDED
#define MARU_CONVENIENCE_H_INCLUDED

#include "maru/c/contexts.h"
#include "maru/c/windows.h"
#include "maru/c/monitors.h"
#include "maru/c/cursors.h"
#include "maru/c/instrumentation.h"

/**
 * @file convenience.h
 * @brief Inline convenience functions for common attribute updates.
 */

#ifdef __cplusplus
extern "C" {
#endif

// Context attribute setters
// N.B. If you are calling a few of those in a row, consider using maru_updateWindow instead.
static inline void maru_setContextInhibitsSystemIdle(MARU_Context *context, bool enabled);

// Window attribute setters
// N.B. If you are calling a few of those in a row, consider using maru_updateWindow instead.
static inline void maru_setWindowTitle(MARU_Window *window, const char *title);
static inline void maru_setWindowSize(MARU_Window *window, MARU_Vec2Dip size);
static inline void maru_setWindowFullscreen(MARU_Window *window, bool enabled);
static inline void maru_setWindowMaximized(MARU_Window *window, bool enabled);
static inline void maru_setWindowCursorMode(MARU_Window *window, MARU_CursorMode mode);
static inline void maru_setWindowCursor(MARU_Window *window, MARU_Cursor *cursor);
static inline void maru_setWindowMonitor(MARU_Window *window, MARU_Monitor *monitor);
static inline void maru_setWindowMinSize(MARU_Window *window, MARU_Vec2Dip min_size);
static inline void maru_setWindowMaxSize(MARU_Window *window, MARU_Vec2Dip max_size);
static inline void maru_setWindowAspectRatio(MARU_Window *window, MARU_Fraction aspect_ratio);
static inline void maru_setWindowResizable(MARU_Window *window, bool enabled);
static inline void maru_setWindowMousePassthrough(MARU_Window *window, bool enabled);
static inline void maru_setWindowTextInputType(MARU_Window *window, MARU_TextInputType type);
static inline void maru_setWindowTextInputRect(MARU_Window *window, MARU_RectDip rect);
static inline void maru_setWindowEventMask(MARU_Window *window, MARU_EventMask mask);

static inline const MARU_UserEventMetrics *maru_getContextEventMetrics(const MARU_Context *context);

static inline const char *maru_getDiagnosticString(MARU_Diagnostic diagnostic);

#ifdef __cplusplus
}
#endif

#include "maru/c/details/convenience_impls.h"

#endif  // MARU_CONVENIENCE_H_INCLUDED
