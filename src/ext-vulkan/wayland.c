#include "maru/c/core.h"
#include "maru/c/ext/vulkan.h"
#include "maru/c/ext/core.h"
#include "maru_internal.h"
#include "maru_api_constraints.h"
#include "ext_vulkan_internal.h"
#include "vulkan_api_constraints.h"
#include <string.h>

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

typedef VkResult (*PFN_vkCreateWaylandSurfaceKHR)(VkInstance instance,
                                                  const VkWaylandSurfaceCreateInfoKHR *pCreateInfo,
                                                  const void *pAllocator, VkSurfaceKHR *pSurface);

extern __attribute__((weak)) MARU_VulkanVoidFunction vkGetInstanceProcAddr(VkInstance instance,
                                                                      const char *pName);

#if defined(_WIN32)
    __declspec(allocate(".maru_ext$a")) extern const void* const _maru_ext_start_marker;
#else
    extern const MARU_ExtensionDescriptor* const __start_maru_ext_hooks;
#endif

#ifndef MARU_INDIRECT_BACKEND
static void _maru_vulkan_auto_init(MARU_Context *ctx, MARU_ExtensionID id);
#endif

#ifndef MARU_INDIRECT_BACKEND
MARU_REGISTER_EXTENSION("vulkan", _maru_vulkan_auto_init)
#endif

static MARU_ExtensionID _maru_vulkan_get_id(MARU_Context *ctx) {
#ifdef MARU_INDIRECT_BACKEND
    (void)ctx;
    return 0; // Placeholder
#else
#if defined(_WIN32)
    return (MARU_ExtensionID)(&_maru_ext_ptr__maru_vulkan_auto_init - (&_maru_ext_start_marker + 1));
#else
    return (MARU_ExtensionID)(&_maru_ext_ptr__maru_vulkan_auto_init - &__start_maru_ext_hooks);
#endif
#endif
}

#ifndef MARU_INDIRECT_BACKEND
static void _maru_vulkan_auto_init(MARU_Context *ctx, MARU_ExtensionID id) {
    (void)id;
    maru_vulkan_enable(ctx, NULL);
}
#endif

typedef struct MARU_VulkanExtContextState_WL {
  #ifdef MARU_INDIRECT_BACKEND
    const MARU_ExtVulkanVTable* vtable;                 // MUST BE THE FIRST MEMBER!
  #endif
  MARU_VkGetInstanceProcAddrFunc vk_loader;
} MARU_VulkanExtContextState_WL;

static const char **_getVkExtensions_WL(MARU_Context *context, uint32_t *out_count) {
  (void)context;
  static const char *extensions[] = {
      "VK_KHR_surface",
      "VK_KHR_wayland_surface",
  };

  *out_count = 2;
  return extensions;
}

static MARU_Status _createVkSurface_WL(MARU_Window *window, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface) {
  MARU_Context *ctx = maru_getWindowContext(window);
  MARU_ExtensionID id = _maru_vulkan_get_id(ctx);
  MARU_VulkanExtContextState_WL *state = (MARU_VulkanExtContextState_WL *)maru_getExtension(ctx, id);

  MARU_VkGetInstanceProcAddrFunc vk_loader = state->vk_loader;
  if (!vk_loader) vk_loader = (MARU_VkGetInstanceProcAddrFunc)vkGetInstanceProcAddr;

  if (!vk_loader) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                                "No Vulkan loader available. Provide vk_loader in maru_vulkan_enable.");
    return MARU_FAILURE;
  }

  PFN_vkCreateWaylandSurfaceKHR create_surface_fn =
      (PFN_vkCreateWaylandSurfaceKHR)vk_loader(instance, "vkCreateWaylandSurfaceKHR");

  if (!create_surface_fn) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                                "vkCreateWaylandSurfaceKHR not found");
    return MARU_FAILURE;
  }

  VkWaylandSurfaceCreateInfoKHR cinfo = {
      .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .display = (struct wl_display *)maru_contextGetNativeHandle(ctx),
      .surface = (struct wl_surface *)maru_windowGetNativeHandle(window),
  };

  VkResult vk_res = create_surface_fn(instance, &cinfo, NULL, out_surface);
  if (vk_res != VK_SUCCESS) {
    MARU_REPORT_DIAGNOSTIC(ctx, MARU_DIAGNOSTIC_VULKAN_FAILURE,
                                "vkCreateWaylandSurfaceKHR failure");
    return MARU_FAILURE;
  }

  return MARU_SUCCESS;
}

#ifdef MARU_INDIRECT_BACKEND
static const MARU_ExtVulkanVTable maru_ext_vk_backend_WL = {
  .getVkExtensions = _getVkExtensions_WL,
  .createVkSurface = _createVkSurface_WL,
};
#endif

static void _maru_vulkan_cleanup_WL(MARU_Context *context, void *state) {
  maru_contextFree(context, state);
}

MARU_Status maru_vulkan_enable_wayland(MARU_Context *context, MARU_VkGetInstanceProcAddrFunc vk_loader) {
  MARU_VULKAN_API_VALIDATE(enable, context, vk_loader);

  MARU_ExtensionID id = _maru_vulkan_get_id(context);
  MARU_VulkanExtContextState_WL *state = (MARU_VulkanExtContextState_WL *)maru_getExtension(context, id);
  if (!state) {
    state = (MARU_VulkanExtContextState_WL *)maru_contextAlloc(context, sizeof(MARU_VulkanExtContextState_WL));
    if (!state) return MARU_FAILURE;
    memset(state, 0, sizeof(MARU_VulkanExtContextState_WL));

    #ifdef MARU_INDIRECT_BACKEND
    state->vtable = &maru_ext_vk_backend_WL;
    #endif
    maru_registerExtension(context, id, state, _maru_vulkan_cleanup_WL);
  }

  if (vk_loader) {
    state->vk_loader = vk_loader;
  }

  return MARU_SUCCESS;
}

#ifndef MARU_INDIRECT_BACKEND
MARU_API MARU_Status maru_vulkan_enable(MARU_Context *context, MARU_VkGetInstanceProcAddrFunc vk_loader) {
  MARU_VULKAN_API_VALIDATE(enable, context, vk_loader);
  return maru_vulkan_enable_wayland(context, vk_loader);
}

MARU_API const char **maru_vulkan_getVkExtensions(MARU_Context *context, uint32_t *out_count) {
  MARU_VULKAN_API_VALIDATE(getVkExtensions, context, out_count);
  return _getVkExtensions_WL(context, out_count);
}

MARU_API MARU_Status maru_vulkan_createVkSurface(MARU_Window *window, 
                                        VkInstance instance,
                                        VkSurfaceKHR *out_surface) {
  MARU_VULKAN_API_VALIDATE(createVkSurface, window, instance, out_surface);
  return _createVkSurface_WL(window, instance, out_surface);
}
#endif
