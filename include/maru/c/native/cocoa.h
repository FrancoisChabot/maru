// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_COCOA_H_INCLUDED
#define MARU_NATIVE_COCOA_H_INCLUDED

#include "maru/c/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

#ifdef MARU_ENABLE_BACKEND_COCOA
typedef struct MARU_CocoaContextHandle {
  void *ns_application;
} MARU_CocoaContextHandle;

typedef struct MARU_CocoaWindowHandle {
  void *ns_window;
} MARU_CocoaWindowHandle;

MARU_API MARU_Status maru_getCocoaContextHandle(MARU_Context *context,
                                                MARU_CocoaContextHandle *out_handle);
MARU_API MARU_Status maru_getCocoaWindowHandle(MARU_Window *window,
                                               MARU_CocoaWindowHandle *out_handle);
#endif

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_COCOA_H_INCLUDED
