// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_EXT_CORE_H_INCLUDED
#define MARU_EXT_CORE_H_INCLUDED

#include <stddef.h>
#include "maru/c/core.h"

/**
 * @file ext/core.h
 * @brief Primitives for implementing MARU extensions.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing the library state and display server connection. */
typedef struct MARU_Context MARU_Context;

/** @brief Function signature for extension cleanup callbacks. */
typedef void (*MARU_ExtensionCleanupCallback)(MARU_Context *context, void *state);

/** @brief Registers a formalized extension vtable with the context. 
 *
 *  The provided cleanup callback will be invoked when the context is destroyed.
 */
MARU_API MARU_Status maru_registerExtension(MARU_Context *context, MARU_ExtensionID id, void *state, MARU_ExtensionCleanupCallback cleanup);

/** @brief Retrieves a registered extension vtable. Returns NULL if not registered. */
MARU_API void *maru_getExtension(const MARU_Context *context, MARU_ExtensionID id);

/** @brief Allocates memory using the context's configured allocator. */
MARU_API void *maru_contextAlloc(MARU_Context *context, size_t size);

/** @brief Frees memory using the context's configured allocator. */
MARU_API void maru_contextFree(MARU_Context *context, void *ptr);

/** @brief Retrieves the native display handle for the context (e.g., wl_display*, Display*, HINSTANCE). */
MARU_API void *maru_contextGetNativeHandle(MARU_Context *context);

/** @brief Retrieves the native window handle (e.g., wl_surface*, Window, HWND). */
MARU_API void *maru_windowGetNativeHandle(MARU_Window *window);

#ifdef __cplusplus
}
#endif

#endif  // MARU_EXT_CORE_H_INCLUDED
