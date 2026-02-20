#include "maru/c/ext/vulkan.h"
#include "maru_internal.h"
#include "maru_api_constraints.h"

#ifdef MARU_ENABLE_VULKAN

#include "../core/windows/windows_internal.h"

typedef enum VkResult {
  VK_SUCCESS = 0,
} VkResult;

#define VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR 1000009000

typedef uint32_t VkFlags;
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HWND__* HWND;

typedef struct VkWin32SurfaceCreateInfoKHR {
  int sType;
  const void* pNext;
  VkFlags flags;
  HINSTANCE hinstance;
  HWND hwnd;
} VkWin32SurfaceCreateInfoKHR;

typedef VkResult (*PFN_vkCreateWin32SurfaceKHR)(VkInstance instance,
                                               const VkWin32SurfaceCreateInfoKHR* pCreateInfo,
                                               const void* pAllocator,
                                               VkSurfaceKHR* pSurface);

typedef MARU_VulkanVoidFunction (*PFN_vkGetInstanceProcAddr)(void* instance, const char* pName);

extern MARU_VulkanVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName);

MARU_Status maru_getVkExtensions_Windows(MARU_Context* context, MARU_ExtensionList* out_list) {
  (void)context;
  static const char* extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_win32_surface",
  };

  out_list->extensions = extensions;
  out_list->count = 2;

  return MARU_SUCCESS;
}

MARU_Status maru_createVkSurface_Windows(MARU_Window* window_handle,
                                         VkInstance instance,
                                         VkSurfaceKHR* out_surface) {
  MARU_Window_Windows* window = (MARU_Window_Windows*)window_handle;
  MARU_Context_Windows* ctx = (MARU_Context_Windows*)window->base.ctx_base;

  MARU_VkGetInstanceProcAddrFunc vk_loader = ctx->base.tuning.vk_loader;

  if (!vk_loader) {
    vk_loader = (MARU_VkGetInstanceProcAddrFunc)vkGetInstanceProcAddr;
  }

  if (!vk_loader) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "No Vulkan loader available. Provide vk_loader in context tuning.");
    return MARU_FAILURE;
  }

  PFN_vkCreateWin32SurfaceKHR create_surface_fn =
      (PFN_vkCreateWin32SurfaceKHR)vk_loader((void*)instance, "vkCreateWin32SurfaceKHR");

  if (!create_surface_fn) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                          "vkCreateWin32SurfaceKHR not found");
    return MARU_FAILURE;
  }

  HINSTANCE hinstance = GetModuleHandleA(NULL);

  VkWin32SurfaceCreateInfoKHR cinfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .hinstance = hinstance,
      .hwnd = window->hwnd,
  };

  VkResult vk_res = create_surface_fn(instance, &cinfo, NULL, out_surface);
  if (vk_res != VK_SUCCESS) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                          "vkCreateWin32SurfaceKHR failure");
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}

#ifndef MARU_INDIRECT_BACKEND

MARU_API MARU_Status maru_getVkExtensions(MARU_Context* context, MARU_ExtensionList* out_list) {
  MARU_API_VALIDATE(getVkExtensions, context, out_list);
  return maru_getVkExtensions_Windows(context, out_list);
}

MARU_API MARU_Status maru_createVkSurface(MARU_Window* window,
                                           VkInstance instance,
                                           VkSurfaceKHR* out_surface) {
  MARU_API_VALIDATE(createVkSurface, window, instance, out_surface);
  return maru_createVkSurface_Windows(window, instance, out_surface);
}

#endif // MARU_INDIRECT_BACKEND

#endif // MARU_ENABLE_VULKAN
