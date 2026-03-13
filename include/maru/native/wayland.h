// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_WAYLAND_H_INCLUDED
#define MARU_NATIVE_WAYLAND_H_INCLUDED

#include "maru/maru.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct wl_display wl_display;
typedef struct wl_surface wl_surface;
typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

/*
 * Borrowed native handles owned by Maru.
 *
 * Do not destroy these objects yourself. Reacquire them if the owning Maru
 * handle is destroyed and recreated.
 */
typedef struct MARU_WaylandContextHandle {
  wl_display *display;
} MARU_WaylandContextHandle;

typedef struct MARU_WaylandWindowHandle {
  wl_display *display;
  wl_surface *surface;
} MARU_WaylandWindowHandle;

/*
 * Returns the Wayland display for a Wayland-backed context.
 *
 * Requires `context` to use MARU_BACKEND_WAYLAND.
 *
 * The returned pointers are borrowed and remain owned by Maru.
 */
MARU_API MARU_WaylandContextHandle
maru_getWaylandContextHandle(const MARU_Context *context);
/*
 * Returns the Wayland display/surface pair for a ready Wayland window.
 *
 * Requires `window` to belong to a MARU_BACKEND_WAYLAND context and to be
 * ready for native use.
 *
 * The returned pointers are borrowed and remain owned by Maru.
 */
MARU_API MARU_WaylandWindowHandle
maru_getWaylandWindowHandle(const MARU_Window *window);

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_WAYLAND_H_INCLUDED
