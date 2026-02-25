#include "maru/c/core.h"
#include "maru/c/native/win32.h"
#include "maru/c/vulkan.h"
#include "maru_internal.h"
#include "vulkan_api_constraints.h"

#ifdef _WIN32
#include <windows.h>
#endif

typedef enum VkResult {
  VK_SUCCESS = 0,
} VkResult;

#define VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR 1000009000

typedef uint32_t VkFlags;
#ifndef _WIN32
typedef struct HINSTANCE__ *HINSTANCE;
typedef struct HWND__ *HWND;
#endif

typedef struct VkWin32SurfaceCreateInfoKHR {
  int sType;
  const void *pNext;
  VkFlags flags;
  HINSTANCE hinstance;
  HWND hwnd;
} VkWin32SurfaceCreateInfoKHR;

typedef VkResult (*PFN_vkCreateWin32SurfaceKHR)(
    VkInstance instance, const VkWin32SurfaceCreateInfoKHR *pCreateInfo,
    const void *pAllocator, VkSurfaceKHR *pSurface);

extern __attribute__((weak)) MARU_VulkanVoidFunction
vkGetInstanceProcAddr(VkInstance instance, const char *pName);

const char **maru_getVkExtensions_Windows(const MARU_Context *context,
                                          uint32_t *out_count) {
  (void)context;
  static const char *extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_win32_surface",
  };

  *out_count = 2;
  return extensions;
}

MARU_Status maru_createVkSurface_Windows(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
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
    return MARU_FAILURE;
  }

  PFN_vkCreateWin32SurfaceKHR create_surface_fn =
      (PFN_vkCreateWin32SurfaceKHR)vk_loader((void *)instance,
                                             "vkCreateWin32SurfaceKHR");

  if (!create_surface_fn) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "vkCreateWin32SurfaceKHR not found");
    return MARU_FAILURE;
  }

#ifdef _WIN32
  HINSTANCE hinstance = GetModuleHandleA(NULL);
#else
  HINSTANCE hinstance = NULL;
#endif

  MARU_Win32WindowHandle win_handle;
  if (maru_getWin32WindowHandle(window, &win_handle) != MARU_SUCCESS) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "Failed to retrieve Win32 window handles");
    return MARU_FAILURE;
  }

  VkWin32SurfaceCreateInfoKHR cinfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .hinstance = hinstance ? hinstance : win_handle.instance,
      .hwnd = win_handle.hwnd,
  };

  VkResult vk_res = create_surface_fn(instance, &cinfo, NULL, out_surface);
  if (vk_res != VK_SUCCESS) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "vkCreateWin32SurfaceKHR failure");
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}

#ifndef MARU_INDIRECT_BACKEND
MARU_API const char **maru_vulkan_getVkExtensions(const MARU_Context *context,
                                                  uint32_t *out_count) {
  MARU_VULKAN_API_VALIDATE(getVkExtensions, context, out_count);
  return maru_getVkExtensions_Windows(context, out_count);
}

MARU_API MARU_Status maru_vulkan_createVkSurface(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_VULKAN_API_VALIDATE(createVkSurface, window, instance, vk_loader,
                           out_surface);
  return maru_createVkSurface_Windows(window, instance, vk_loader, out_surface);
}
#endif
