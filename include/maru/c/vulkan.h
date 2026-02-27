// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_VULKAN_H_INCLUDED
#define MARU_VULKAN_H_INCLUDED

#include <stdint.h>
#include "maru/c/core.h"
#include "maru/c/tuning.h"

/**
 * @file vulkan.h
 * @brief Vulkan surface creation and instance extension discovery.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing the library state and display server connection. */
typedef struct MARU_Context MARU_Context;
/** @brief Opaque handle representing an OS-level window. */
typedef struct MARU_Window MARU_Window;

// Vulkan forward declarations, so that include order doesn't matter for users
// of the library.

/** @brief Opaque handle to a Vulkan surface. */
typedef struct VkSurfaceKHR_T *VkSurfaceKHR;

/** @brief Retrieves the list of Vulkan instance extensions required by MARU. 

The returned list is valid until the next call to maru_pumpEvents().
*/
const char **maru_getVkExtensions(const MARU_Context *context, uint32_t *out_count);

/** @brief Creates a Vulkan surface for the specified window. 
*/
MARU_Status maru_createVkSurface(MARU_Window *window, 
                                        VkInstance instance,
                                        MARU_VkGetInstanceProcAddrFunc vk_loader,
                                        VkSurfaceKHR *out_surface);

#ifdef __cplusplus
}
#endif

#endif  // MARU_VULKAN_H_INCLUDED
