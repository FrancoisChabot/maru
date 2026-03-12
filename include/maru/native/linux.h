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

MARU_API MARU_LinuxContextHandle
maru_getLinuxContextHandle(MARU_Context *context);
MARU_API MARU_LinuxWindowHandle
maru_getLinuxWindowHandle(MARU_Window *window);

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_LINUX_H_INCLUDED
