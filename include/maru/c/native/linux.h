// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_LINUX_H_INCLUDED
#define MARU_NATIVE_LINUX_H_INCLUDED

#include "maru/c/core.h"
#include "maru/c/native/wayland.h"
#include "maru/c/native/x11.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

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

MARU_API MARU_Status maru_getLinuxContextHandle(MARU_Context *context,
                                                MARU_LinuxContextHandle *out_handle);
MARU_API MARU_Status maru_getLinuxWindowHandle(MARU_Window *window,
                                               MARU_LinuxWindowHandle *out_handle);

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_LINUX_H_INCLUDED
