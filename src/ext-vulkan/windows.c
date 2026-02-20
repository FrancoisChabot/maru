#include "maru/c/core.h"
#include "maru/c/ext/vulkan.h"
#include "maru/c/ext/core.h"
#include "maru_internal.h"
#include "maru_api_constraints.h"
#include "ext_vulkan_internal.h"
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
typedef struct HINSTANCE__* HINSTANCE;
typedef struct HWND__* HWND;
#endif

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

extern __attribute__((weak)) MARU_VulkanVoidFunction vkGetInstanceProcAddr(VkInstance instance, const char* pName);

typedef struct MARU_VulkanExtContextState_Win32 {
  #ifdef MARU_INDIRECT_BACKEND
    const MARU_ExtVulkanVTable* vtable;
  #endif
  MARU_VkGetInstanceProcAddrFunc vk_loader;
} MARU_VulkanExtContextState_Win32;

static const char** _getVkExtensions_Windows(MARU_Context* context, uint32_t* out_count) {
  (void)context;
  static const char* extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_win32_surface",
  };

  *out_count = 2;
  return (const char**)extensions;
}

static MARU_Status _createVkSurface_Windows(MARU_Window* window,
                                         VkInstance instance,
                                         VkSurfaceKHR* out_surface) {
  MARU_Context* ctx = maru_getWindowContext(window);
  MARU_VulkanExtContextState_Win32 *state = (MARU_VulkanExtContextState_Win32 *)maru_getExtension(ctx, MARU_EXT_VULKAN);

  MARU_VkGetInstanceProcAddrFunc vk_loader = state->vk_loader;

  if (!vk_loader) {
    vk_loader = (MARU_VkGetInstanceProcAddrFunc)vkGetInstanceProcAddr;
  }

  if (!vk_loader) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                           "No Vulkan loader available. Provide vk_loader in maru_vulkan_enable.");
    return MARU_FAILURE;
  }

  PFN_vkCreateWin32SurfaceKHR create_surface_fn =
      (PFN_vkCreateWin32SurfaceKHR)vk_loader((void*)instance, "vkCreateWin32SurfaceKHR");

  if (!create_surface_fn) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                          "vkCreateWin32SurfaceKHR not found");
    return MARU_FAILURE;
  }

  HINSTANCE hinstance = GetModuleHandleA(NULL);

  VkWin32SurfaceCreateInfoKHR cinfo = {
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .hinstance = hinstance,
      .hwnd = (HWND)maru_windowGetNativeHandle(window),
  };

  VkResult vk_res = create_surface_fn(instance, &cinfo, NULL, out_surface);
  if (vk_res != VK_SUCCESS) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                          "vkCreateWin32SurfaceKHR failure");
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}

#ifdef MARU_INDIRECT_BACKEND
static const MARU_ExtVulkanVTable maru_ext_vk_backend_Win32 = {
  .getVkExtensions = (const char ** (*)(MARU_Context *, uint32_t *))_getVkExtensions_Windows,
  .createVkSurface = _createVkSurface_Windows,
};
#endif

static void _maru_vulkan_cleanup_Win32(MARU_Context *context, void *state) {
  maru_contextFree(context, state);
}

MARU_API MARU_Status maru_vulkan_enable(MARU_Context *context, MARU_VkGetInstanceProcAddrFunc vk_loader) {
  MARU_VULKAN_API_VALIDATE(enable, context, vk_loader);

  MARU_VulkanExtContextState_Win32 *state = (MARU_VulkanExtContextState_Win32 *)maru_contextAlloc(context, sizeof(MARU_VulkanExtContextState_Win32));
  if (!state) return MARU_FAILURE;

  #ifdef MARU_INDIRECT_BACKEND
  state->vtable = &maru_ext_vk_backend_Win32;
  #endif

  state->vk_loader = vk_loader;
  maru_registerExtension(context, MARU_EXT_VULKAN, state, _maru_vulkan_cleanup_Win32);
  return MARU_SUCCESS;
}

#ifndef MARU_INDIRECT_BACKEND
MARU_API const char **maru_vulkan_getVkExtensions(MARU_Context *context, uint32_t *out_count) {
  MARU_VULKAN_API_VALIDATE(getVkExtensions, context, out_count);
  return (const char **)_getVkExtensions_Windows(context, out_count);
}

MARU_API MARU_Status maru_vulkan_createVkSurface(MARU_Window *window, 
                                        VkInstance instance,
                                        VkSurfaceKHR *out_surface) {
  MARU_VULKAN_API_VALIDATE(createVkSurface, window, instance, out_surface);
  return _createVkSurface_Windows(window, instance, out_surface);
}
#endif
