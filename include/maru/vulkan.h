// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_VULKAN_H_INCLUDED
#define MARU_VULKAN_H_INCLUDED

#include <vulkan/vulkan.h>

#include "maru/maru.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef PFN_vkGetInstanceProcAddr MARU_VkGetInstanceProcAddrFunc;

/*
 * Returns a borrowed extension-name array. Backends may point this at static
 * storage; treat it as read-only and valid for at least the lifetime of the
 * context.
 */
MARU_API MARU_Status maru_getVkExtensions(const MARU_Context* context,
                                          MARU_StringList* out_list);
/* Requires a ready window. */
MARU_API MARU_Status maru_createVkSurface(MARU_Window* window,
                                          VkInstance instance,
                                          MARU_VkGetInstanceProcAddrFunc vk_loader,
                                          VkSurfaceKHR* out_surface);

#ifdef __cplusplus
}
#endif

#endif
