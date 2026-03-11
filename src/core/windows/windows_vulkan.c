// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru/c/vulkan.h"

// Vulkan types and function pointers needed for the implementation.
// We define them locally to avoid a hard dependency on Vulkan headers.

typedef enum VkResult {
  VK_SUCCESS = 0,
} VkResult;

#define VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR 1000009000

typedef struct VkWin32SurfaceCreateInfoKHR {
  int sType;
  const void *pNext;
  uint32_t flags;
  HINSTANCE hinstance;
  HWND hwnd;
} VkWin32SurfaceCreateInfoKHR;

typedef VkResult (*PFN_vkCreateWin32SurfaceKHR)(
    VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo,
    const void *pAllocator, VkSurfaceKHR *pSurface);

MARU_Status maru_getVkExtensions_Windows(const MARU_Context *context, MARU_VkExtensionList *out_list) {
  (void)context;
  static const char *extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_win32_surface",
  };

  if (out_list) {
    out_list->count = 2;
    out_list->names = (const char *const *)extensions;
  }
  return MARU_SUCCESS;
}

MARU_Status maru_createVkSurface_Windows(MARU_Window *window, VkInstance instance, MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)win->base.ctx_base;

  if (!vk_loader) {
    MARU_REPORT_DIAGNOSTIC(
        (const MARU_Context *)&ctx->base, MARU_DIAGNOSTIC_VULKAN_FAILURE,
        "No Vulkan loader available. Pass vk_loader to maru_createVkSurface.");
    return MARU_FAILURE;
  }

  PFN_vkCreateWin32SurfaceKHR create_surface_fn =
      (PFN_vkCreateWin32SurfaceKHR)vk_loader(instance, "vkCreateWin32SurfaceKHR");

  if (!create_surface_fn) {
    MARU_REPORT_DIAGNOSTIC((const MARU_Context *)&ctx->base, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "vkCreateWin32SurfaceKHR not found");
    return MARU_FAILURE;
  }

  VkWin32SurfaceCreateInfoKHR cinfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .hinstance = ctx->instance,
      .hwnd = win->hwnd,
  };

  VkResult vk_res = create_surface_fn(instance, &cinfo, NULL, out_surface);
  if (vk_res != VK_SUCCESS) {
    MARU_REPORT_DIAGNOSTIC((const MARU_Context *)&ctx->base, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "vkCreateWin32SurfaceKHR failure");
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}
