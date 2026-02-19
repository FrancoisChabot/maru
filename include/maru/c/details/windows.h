// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DETAILS_WINDOWS_H_INCLUDED
#define MARU_DETAILS_WINDOWS_H_INCLUDED

#include "maru/c/windows.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Internal representation of MARU_Window. 
    CHANGING THIS REQUIRES A MAJOR VERSION BUMP
*/
typedef struct MARU_WindowExposed {
  void *userdata;
  uint64_t flags;
} MARU_WindowExposed;

static inline void *maru_getWindowUserdata(const MARU_Window *window) {
  return ((const MARU_WindowExposed *)window)->userdata;
}

static inline void maru_setWindowUserdata(MARU_Window *window, void *userdata) {
  ((MARU_WindowExposed *)window)->userdata = userdata;
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

#ifdef __cplusplus
}
#endif

#endif  // MARU_DETAILS_WINDOWS_H_INCLUDED
