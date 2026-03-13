// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_WIN32_H_INCLUDED
#define MARU_NATIVE_WIN32_H_INCLUDED

#include "maru/maru.h"

#ifndef _WINDOWS_
#ifndef HWND
typedef struct HWND__* HWND;
#endif

#ifndef HINSTANCE
typedef struct HINSTANCE__* HINSTANCE;
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

/*
 * Borrowed native handles owned by Maru.
 *
 * Do not destroy these objects yourself. Reacquire them if the owning Maru
 * handle is destroyed and recreated.
 */
typedef struct MARU_Win32ContextHandle {
  HINSTANCE instance;
} MARU_Win32ContextHandle;

typedef struct MARU_Win32WindowHandle {
  HINSTANCE instance;
  HWND hwnd;
} MARU_Win32WindowHandle;

/*
 * Returns the Win32 module instance for a Windows-backed context.
 *
 * Requires `context` to use MARU_BACKEND_WINDOWS.
 *
 * The returned handles are borrowed and remain owned by Maru.
 */
MARU_API MARU_Win32ContextHandle
maru_getWin32ContextHandle(const MARU_Context *context);
/*
 * Returns the Win32 instance/HWND pair for a ready Windows window.
 *
 * Requires `window` to belong to a MARU_BACKEND_WINDOWS context and to be
 * ready for native use.
 *
 * The returned handles are borrowed and remain owned by Maru.
 */
MARU_API MARU_Win32WindowHandle
maru_getWin32WindowHandle(const MARU_Window *window);

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_WIN32_H_INCLUDED
