// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WINDOW_H_INCLUDED
#define MARU_WINDOW_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/geometry.h"

/**
 * @file windows.h
 * @brief Window management, lifecycle, attributes, and core state.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_Window MARU_Window;

/** @brief Runtime state flags for a window. */
typedef enum MARU_WindowStateFlagBits {
  MARU_WINDOW_STATE_LOST = 1ULL << 0,
  MARU_WINDOW_STATE_READY = 1ULL << 1,
  MARU_WINDOW_STATE_FOCUSED = 1ULL << 2,
  MARU_WINDOW_STATE_MAXIMIZED = 1ULL << 3,
  MARU_WINDOW_STATE_FULLSCREEN = 1ULL << 4,
} MARU_WindowStateFlagBits;

/** @brief Live-updatable window properties. */
typedef struct MARU_WindowAttributes {
  const char *title;
} MARU_WindowAttributes;

/** @brief Parameters for maru_createWindow(). */
typedef struct MARU_WindowCreateInfo {
  MARU_WindowAttributes attributes;
  void *userdata;
} MARU_WindowCreateInfo;

/** @brief Default initialization macro for MARU_WindowCreateInfo. */
#define MARU_WINDOW_CREATE_INFO_DEFAULT                         \
  {.attributes = {.title = "MARU Window"},                       \
   .userdata = NULL                                             \
  }

/** @brief Retrieves the user-defined data pointer associated with a window. */
static inline void *maru_getWindowUserdata(const MARU_Window *window);

/** @brief Sets the user-defined data pointer associated with a window. */
static inline void maru_setWindowUserdata(MARU_Window *window, void *userdata);

/** @brief Checks if the window has been lost due to an unrecoverable error. */
static inline bool maru_isWindowLost(const MARU_Window *window);

/** @brief Checks if the window is fully initialized and ready for use. */
static inline bool maru_isWindowReady(const MARU_Window *window);

/** @brief Checks if the window currently has input focus. */
static inline bool maru_isWindowFocused(const MARU_Window *window);

/** @brief Checks if the window is currently maximized. */
static inline bool maru_isWindowMaximized(const MARU_Window *window);

/** @brief Checks if the window is currently in fullscreen mode. */
static inline bool maru_isWindowFullscreen(const MARU_Window *window);

#include "maru/c/details/windows.h"

#ifdef __cplusplus
}
#endif

#endif  // MARU_WINDOW_H_INCLUDED
