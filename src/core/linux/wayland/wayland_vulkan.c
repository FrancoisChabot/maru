// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru/native/wayland.h"
#include "maru/maru.h"
#include "maru_internal.h"
#include "maru_api_constraints.h"

// Vulkan types and function pointers needed for the implementation.
// We define them locally to avoid a hard dependency on Vulkan headers.

typedef enum VkResult {
  VK_SUCCESS = 0,
} VkResult;

#define VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR 1000006000

typedef struct VkWaylandSurfaceCreateInfoKHR {
  int sType;
  const void *pNext;
  uint32_t flags;
  struct wl_display *display;
  struct wl_surface *surface;
} VkWaylandSurfaceCreateInfoKHR;

typedef VkResult (*PFN_vkCreateWaylandSurfaceKHR)(
    VkInstance instance, const VkWaylandSurfaceCreateInfoKHR *pCreateInfo,
    const void *pAllocator, VkSurfaceKHR *pSurface);

MARU_Status maru_getVkExtensions_WL(const MARU_Context *context,
                                    MARU_StringList *out_list) {
  (void)context;
  static const char *extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_wayland_surface",
  };

  out_list->count = 2;
  out_list->strings = (const char *const *)extensions;
  return MARU_SUCCESS;
}

MARU_Status maru_createVkSurface_WL(MARU_Window *window, VkInstance instance,
                                    MARU_VkGetInstanceProcAddrFunc vk_loader,
                                    VkSurfaceKHR *out_surface) {
  const MARU_Context *ctx = maru_getWindowContext(window);

  PFN_vkCreateWaylandSurfaceKHR create_surface_fn =
      (PFN_vkCreateWaylandSurfaceKHR)vk_loader(instance,
                                               "vkCreateWaylandSurfaceKHR");

  if (!create_surface_fn) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "vkCreateWaylandSurfaceKHR not found");
    return MARU_FAILURE;
  }

  MARU_WaylandWindowHandle wl_handle = maru_getWaylandWindowHandle(window);

  VkWaylandSurfaceCreateInfoKHR cinfo = {
      .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .display = wl_handle.display,
      .surface = wl_handle.surface,
  };

  VkResult vk_res = create_surface_fn(instance, &cinfo, NULL, out_surface);
  if (vk_res != VK_SUCCESS) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "vkCreateWaylandSurfaceKHR failure");
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}

#ifndef MARU_INDIRECT_BACKEND
MARU_API MARU_Status maru_getVkExtensions(const MARU_Context *context,
                                         MARU_StringList *out_list) {
  MARU_API_VALIDATE(getVkExtensions, context, out_list);
  MARU_RETURN_ON_ERROR(_maru_status_if_context_lost(context));
  return maru_getVkExtensions_WL(context, out_list);
}

MARU_API MARU_Status maru_createVkSurface(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_API_VALIDATE(createVkSurface, window, instance, vk_loader,
                           out_surface);
  MARU_RETURN_ON_ERROR(_maru_status_if_window_context_lost(window));
  MARU_API_VALIDATE_LIVE(createVkSurface, window, instance, vk_loader,
                         out_surface);
  return maru_createVkSurface_WL(window, instance, vk_loader, out_surface);
}
#endif
