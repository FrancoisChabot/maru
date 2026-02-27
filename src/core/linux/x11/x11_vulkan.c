#include "maru/c/native/linux.h"
#include "maru/c/vulkan.h"
#include "maru_internal.h"
#include "vulkan_api_constraints.h"
#include <stdio.h>

// Vulkan types and function pointers needed for the implementation.
// We define them locally to avoid a hard dependency on Vulkan headers.

typedef enum VkResult {
  VK_SUCCESS = 0,
} VkResult;

#define VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR 1000004000

typedef struct VkXlibSurfaceCreateInfoKHR {
  int sType;
  const void *pNext;
  uint32_t flags;
  Display *dpy;
  Window window;
} VkXlibSurfaceCreateInfoKHR;

typedef VkResult (*PFN_vkCreateXlibSurfaceKHR)(
    VkInstance instance, const VkXlibSurfaceCreateInfoKHR *pCreateInfo,
    const void *pAllocator, VkSurfaceKHR *pSurface);

extern __attribute__((weak)) MARU_VulkanVoidFunction
vkGetInstanceProcAddr(VkInstance instance, const char *pName);

const char **maru_getVkExtensions_X11(const MARU_Context *context,
                                      uint32_t *out_count) {
  (void)context;
  static const char *extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_xlib_surface",
  };

  *out_count = 2;
  return extensions;
}

MARU_Status maru_createVkSurface_X11(MARU_Window *window, VkInstance instance,
                                     MARU_VkGetInstanceProcAddrFunc vk_loader,
                                     VkSurfaceKHR *out_surface) {
  fprintf(stderr, "[X11 DEBUG] maru_createVkSurface_X11 started\n");
  MARU_Context *ctx = maru_getWindowContext(window);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)ctx;

  if (!vk_loader) {
    vk_loader = ctx_base->tuning.vulkan.vk_loader;
  }
  if (!vk_loader) {
    vk_loader = (MARU_VkGetInstanceProcAddrFunc)vkGetInstanceProcAddr;
  }

  if (!vk_loader) {
    MARU_REPORT_DIAGNOSTIC(
        ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
        "No Vulkan loader available. Configure tuning.vulkan.vk_loader or pass vk_loader.");
    fprintf(stderr, "[X11 DEBUG] No Vulkan loader available\n");
    return MARU_FAILURE;
  }

  PFN_vkCreateXlibSurfaceKHR create_surface_fn =
      (PFN_vkCreateXlibSurfaceKHR)vk_loader(instance,
                                               "vkCreateXlibSurfaceKHR");

  if (!create_surface_fn) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "vkCreateXlibSurfaceKHR not found");
    fprintf(stderr, "[X11 DEBUG] vkCreateXlibSurfaceKHR not found\n");
    return MARU_FAILURE;
  }

  MARU_X11WindowHandle x11_handle;
  if (maru_getX11WindowHandle(window, &x11_handle) != MARU_SUCCESS) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "Failed to retrieve X11 window handles");
    fprintf(stderr, "[X11 DEBUG] Failed to retrieve X11 window handles\n");
    return MARU_FAILURE;
  }

  fprintf(stderr, "[X11 DEBUG] Creating surface for dpy=%p, win=0x%lx\n", 
          (void*)x11_handle.display, (unsigned long)x11_handle.window);

  VkXlibSurfaceCreateInfoKHR cinfo = {
      .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .dpy = x11_handle.display,
      .window = x11_handle.window,
  };

  VkResult vk_res = create_surface_fn(instance, &cinfo, NULL, out_surface);
  if (vk_res != VK_SUCCESS) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "vkCreateXlibSurfaceKHR failure");
    fprintf(stderr, "[X11 DEBUG] vkCreateXlibSurfaceKHR failure (res=%d)\n", (int)vk_res);
    return MARU_FAILURE;
  }

  fprintf(stderr, "[X11 DEBUG] maru_createVkSurface_X11 success\n");
  return MARU_SUCCESS;
}

#ifndef MARU_INDIRECT_BACKEND
MARU_API const char **maru_vulkan_getVkExtensions(const MARU_Context *context,
                                                  uint32_t *out_count) {
  MARU_VULKAN_API_VALIDATE(getVkExtensions, context, out_count);
  return maru_getVkExtensions_X11(context, out_count);
}

MARU_API MARU_Status maru_vulkan_createVkSurface(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_VULKAN_API_VALIDATE(createVkSurface, window, instance, vk_loader,
                           out_surface);
  return maru_createVkSurface_X11(window, instance, vk_loader, out_surface);
}
#endif
