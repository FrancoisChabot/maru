// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_X11_H_INCLUDED
#define MARU_NATIVE_X11_H_INCLUDED

#include "maru/maru.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MARU_ENABLE_BACKEND_X11

typedef struct _XDisplay Display;
typedef unsigned long Window;

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

/*
 * Borrowed native handles owned by Maru.
 *
 * Do not close or destroy these objects yourself. Reacquire them if the
 * owning Maru handle is destroyed and recreated.
 */
typedef struct MARU_X11ContextHandle {
  Display *display;
} MARU_X11ContextHandle;

typedef struct MARU_X11WindowHandle {
  Display *display;
  Window window;
} MARU_X11WindowHandle;

/*
 * Returns the X11 Display for an X11-backed context.
 *
 * Requires `context` to use MARU_BACKEND_X11.
 *
 * The returned pointers are borrowed and remain owned by Maru.
 */
MARU_API MARU_X11ContextHandle
maru_getX11ContextHandle(const MARU_Context *context);
/*
 * Returns the X11 Display/Window pair for a ready X11 window.
 *
 * Requires `window` to belong to a MARU_BACKEND_X11 context and to be ready
 * for native use.
 *
 * The returned handles are borrowed and remain owned by Maru.
 */
MARU_API MARU_X11WindowHandle
maru_getX11WindowHandle(const MARU_Window *window);
/*
 * Reports whether the current X11 connection/compositor supports Maru's
 * extended frame-sync path.
 *
 * Requires `context` to use MARU_BACKEND_X11.
 */
MARU_API bool maru_getX11SupportsExtendedFrameSync(const MARU_Context *context);

#endif

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_X11_H_INCLUDED
