// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_WAYLAND_H_INCLUDED
#define MARU_NATIVE_WAYLAND_H_INCLUDED

#include "maru/c/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wl_display wl_display;
typedef struct wl_surface wl_surface;
typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

typedef struct MARU_WaylandContextHandle {
  wl_display *display;
} MARU_WaylandContextHandle;

typedef struct MARU_WaylandWindowHandle {
  wl_display *display;
  wl_surface *surface;
} MARU_WaylandWindowHandle;

MARU_API MARU_Status maru_getWaylandContextHandle(MARU_Context *context,
                                                  MARU_WaylandContextHandle *out_handle);
MARU_API MARU_Status maru_getWaylandWindowHandle(MARU_Window *window,
                                                 MARU_WaylandWindowHandle *out_handle);

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_WAYLAND_H_INCLUDED
