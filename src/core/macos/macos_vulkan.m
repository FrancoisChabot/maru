// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import <Cocoa/Cocoa.h>
#import <QuartzCore/CAMetalLayer.h>

// Vulkan types and function pointers needed for the implementation.
// We define them locally to avoid a hard dependency on Vulkan headers.

typedef enum VkResult {
    VK_SUCCESS = 0,
} VkResult;

#define VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT 1000217000

typedef struct VkMetalSurfaceCreateInfoEXT {
    int sType;
    const void *pNext;
    uint32_t flags;
    const CAMetalLayer *pLayer;
} VkMetalSurfaceCreateInfoEXT;

typedef VkResult (*PFN_vkCreateMetalSurfaceEXT)(
    VkInstance instance, const VkMetalSurfaceCreateInfoEXT *pCreateInfo,
    const void *pAllocator, VkSurfaceKHR *pSurface);

const char **maru_getVkExtensions_Cocoa(const MARU_Context *context, uint32_t *out_count) {
    static const char *extensions[] = {
        "VK_KHR_surface",
        "VK_EXT_metal_surface"
    };
    *out_count = 2;
    return extensions;
}

MARU_Status maru_createVkSurface_Cocoa(MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
    MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;

    if (!vk_loader) {
        return MARU_FAILURE;
    }

    PFN_vkCreateMetalSurfaceEXT createMetalSurface = (PFN_vkCreateMetalSurfaceEXT)vk_loader(instance, "vkCreateMetalSurfaceEXT");
    if (!createMetalSurface) return MARU_FAILURE;

    VkMetalSurfaceCreateInfoEXT createInfo = {
        .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
        .pNext = NULL,
        .flags = 0,
        .pLayer = (const CAMetalLayer *)win->ns_layer
    };

    if (createMetalSurface(instance, &createInfo, NULL, out_surface) != VK_SUCCESS) {
        return MARU_FAILURE;
    }

    return MARU_SUCCESS;
}
