// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_X11_H_INCLUDED
#define MARU_NATIVE_X11_H_INCLUDED

#include "maru/c/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _XDisplay Display;
typedef unsigned long Window;

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

typedef struct MARU_X11ContextHandle {
  Display *display;
} MARU_X11ContextHandle;

typedef struct MARU_X11WindowHandle {
  Display *display;
  Window window;
} MARU_X11WindowHandle;

MARU_API MARU_Status maru_getX11ContextHandle(MARU_Context *context,
                                              MARU_X11ContextHandle *out_handle);
MARU_API MARU_Status maru_getX11WindowHandle(MARU_Window *window,
                                             MARU_X11WindowHandle *out_handle);

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_X11_H_INCLUDED
