// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_NATIVE_COCOA_H_INCLUDED
#define MARU_NATIVE_COCOA_H_INCLUDED

#include "maru/maru.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MARU_ENABLE_BACKEND_COCOA

typedef struct MARU_Context MARU_Context;
typedef struct MARU_Window MARU_Window;

/*
 * Borrowed native handles owned by Maru.
 *
 * Do not release or destroy these Objective-C objects yourself. Reacquire them
 * if the owning Maru handle is destroyed and recreated.
 */
typedef struct MARU_CocoaContextHandle {
  void *ns_application;
} MARU_CocoaContextHandle;

typedef struct MARU_CocoaWindowHandle {
  void *ns_window;
  void *ns_view;
  void *ns_layer;
} MARU_CocoaWindowHandle;

/*
 * Returns the NSApplication pointer for a Cocoa-backed context.
 *
 * Requires `context` to use MARU_BACKEND_COCOA.
 *
 * The returned pointers are borrowed and remain owned by Maru.
 */
MARU_API MARU_CocoaContextHandle
maru_getCocoaContextHandle(const MARU_Context *context);
/*
 * Returns the NSWindow pointer for a ready Cocoa window.
 *
 * Requires `window` to belong to a MARU_BACKEND_COCOA context and to be ready
 * for native use.
 *
 * The returned pointers are borrowed and remain owned by Maru.
 */
MARU_API MARU_CocoaWindowHandle
maru_getCocoaWindowHandle(const MARU_Window *window);

#endif

#ifdef __cplusplus
}
#endif

#endif  // MARU_NATIVE_COCOA_H_INCLUDED
