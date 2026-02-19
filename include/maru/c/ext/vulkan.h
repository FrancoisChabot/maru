// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_EXT_VULKAN_H_INCLUDED
#define MARU_EXT_VULKAN_H_INCLUDED

#include <stdint.h>

#include "maru/c/core.h"

/**
 * @file vulkan.h
 * @brief Vulkan surface creation and instance extension discovery.
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MARU_ENABLE_VULKAN

/** @brief Opaque handle representing the library state and display server connection. */
typedef struct MARU_Context MARU_Context;
/** @brief Opaque handle representing an OS-level window. */
typedef struct MARU_Window MARU_Window;

// Vulkan forward declarations, so that include order doesn't matter for users
// of the library.

/** @brief Opaque handle to a Vulkan surface. */
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
/** @brief Opaque handle to a Vulkan instance. */
typedef struct VkInstance_T *VkInstance;

/** @brief A transient list of Vulkan extension strings. */
typedef struct MARU_ExtensionList {
  const char *const *extensions;
  uint32_t count;
} MARU_ExtensionList;

/** @brief Retrieves the list of Vulkan instance extensions required by MARU. 

The returned list is valid until the next call to maru_pumpEvents().
*/
MARU_Status maru_getVkExtensions(MARU_Context *context, MARU_ExtensionList *out_list);

/** @brief Generic function pointer for Vulkan return types. */
/** @brief Creates a Vulkan surface for the specified window. 

N.B. If you are using a custom loader, you can provide it with via context tuning
*/
MARU_Status maru_createVkSurface(MARU_Window *window, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface);

#endif  // MARU_ENABLE_VULKAN

#ifdef __cplusplus
}
#endif

#endif  // MARU_EXT_VULKAN_H_INCLUDED
