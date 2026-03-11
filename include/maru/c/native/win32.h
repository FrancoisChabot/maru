// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_WIN32_H_INCLUDED
#define MARU_NATIVE_WIN32_H_INCLUDED

#include "maru/c/core.h"

#ifdef _WIN32
#include <windows.h>
#else
typedef struct HINSTANCE__ *HINSTANCE;
typedef struct HWND__ *HWND;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

#ifdef MARU_ENABLE_BACKEND_WINDOWS
typedef struct MARU_Win32ContextHandle {
  HINSTANCE instance;
} MARU_Win32ContextHandle;

typedef struct MARU_Win32WindowHandle {
  HINSTANCE instance;
  HWND hwnd;
} MARU_Win32WindowHandle;

MARU_API MARU_Win32ContextHandle
maru_getWin32ContextHandle(MARU_Context *context);
MARU_API MARU_Win32WindowHandle
maru_getWin32WindowHandle(MARU_Window *window);
#endif

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_WIN32_H_INCLUDED
