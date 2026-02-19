#include "maru/c/ext/vulkan.h"
#include "maru_internal.h"
#include "maru_api_constraints.h"
#include "wayland_internal.h"

#ifdef MARU_ENABLE_VULKAN

// Vulkan types and function pointers needed for the implementation.
// We define them locally to avoid a hard dependency on Vulkan headers.

typedef enum VkResult {
  VK_SUCCESS = 0,
} VkResult;

#define VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR 1000006000
#define VK_NULL_HANDLE 0

typedef struct VkWaylandSurfaceCreateInfoKHR {
  int sType;
  const void *pNext;
  uint32_t flags;
  struct wl_display *display;
  struct wl_surface *surface;
} VkWaylandSurfaceCreateInfoKHR;

typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(VkInstance instance, const char *pName);
typedef VkResult (*PFN_vkCreateWaylandSurfaceKHR)(VkInstance instance,
                                                  const VkWaylandSurfaceCreateInfoKHR *pCreateInfo,
                                                  const void *pAllocator, VkSurfaceKHR *pSurface);

extern __attribute__((weak)) PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance,
                                                                      const char *pName);

MARU_Status maru_getVkExtensions_WL(MARU_Context *context, MARU_ExtensionList *out_list) {
  (void)context;
  static const char *extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_wayland_surface",
  };

  out_list->extensions = extensions;
  out_list->count = 2;

  return MARU_SUCCESS;
}

MARU_Status maru_createVkSurface_WL(MARU_Window *window_handle, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface) {
  MARU_Window_WL *window = (MARU_Window_WL *)window_handle;
  MARU_Context_WL *ctx = (MARU_Context_WL *)window->base.ctx_base;

  PFN_vkGetInstanceProcAddr vk_loader = (PFN_vkGetInstanceProcAddr)ctx->base.tuning.vk_loader;
  if (!vk_loader) vk_loader = vkGetInstanceProcAddr;

  if (!vk_loader) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                                "No Vulkan loader available. Provide vk_loader in context tuning or link against Vulkan.");
    return MARU_FAILURE;
  }

  PFN_vkCreateWaylandSurfaceKHR create_surface_fn =
      (PFN_vkCreateWaylandSurfaceKHR)vk_loader(instance, "vkCreateWaylandSurfaceKHR");

  if (!create_surface_fn) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                                "vkCreateWaylandSurfaceKHR not found");
    return MARU_FAILURE;
  }

  VkWaylandSurfaceCreateInfoKHR cinfo = {
      .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .display = ctx->wl.display,
      .surface = window->wl.surface,
  };

  VkResult vk_res = create_surface_fn(instance, &cinfo, NULL, out_surface);
  if (vk_res != VK_SUCCESS) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                                "vkCreateWaylandSurfaceKHR failure");
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}

#ifndef MARU_INDIRECT_BACKEND

MARU_API MARU_Status maru_getVkExtensions(MARU_Context *context, MARU_ExtensionList *out_list) {
  MARU_API_VALIDATE(getVkExtensions, context, out_list);
  return maru_getVkExtensions_WL(context, out_list);
}

MARU_API MARU_Status maru_createVkSurface(MARU_Window *window, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface) {
  MARU_API_VALIDATE(createVkSurface, window, instance, out_surface);
  return maru_createVkSurface_WL(window, instance, out_surface);
}

#endif // MARU_INDIRECT_BACKEND

#endif // MARU_ENABLE_VULKAN
