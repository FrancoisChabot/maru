// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_LINUX_H_INCLUDED
#define MARU_NATIVE_LINUX_H_INCLUDED

#include "maru/maru.h"
#include "maru/native/wayland.h"
#include "maru/native/x11.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

/*
 * Linux-dispatched borrowed native handles owned by Maru.
 *
 * Inspect `backend` first, then read the matching union arm. Do not destroy
 * the underlying native objects yourself.
 */
typedef struct MARU_LinuxContextHandle {
  MARU_BackendType backend;
  union {
    MARU_WaylandContextHandle wayland;
    MARU_X11ContextHandle x11;
  };
} MARU_LinuxContextHandle;

typedef struct MARU_LinuxWindowHandle {
  MARU_BackendType backend;
  union {
    MARU_WaylandWindowHandle wayland;
    MARU_X11WindowHandle x11;
  };
} MARU_LinuxWindowHandle;

/*
 * Returns the native Linux context handle for the active backend.
 *
 * Requires `context` to use MARU_BACKEND_WAYLAND or MARU_BACKEND_X11.
 *
 * Inspect `backend` before reading the union. The returned handles are
 * borrowed and remain owned by Maru.
 */
MARU_API MARU_LinuxContextHandle
maru_getLinuxContextHandle(MARU_Context *context);
/*
 * Returns the native Linux window handle for the active backend.
 *
 * Requires `window` to belong to a Wayland- or X11-backed context and to be
 * ready for native use.
 *
 * Inspect `backend` before reading the union. The returned handles are
 * borrowed and remain owned by Maru.
 */
MARU_API MARU_LinuxWindowHandle
maru_getLinuxWindowHandle(MARU_Window *window);

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_LINUX_H_INCLUDED
