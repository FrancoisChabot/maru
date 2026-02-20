#include "maru/c/core.h"
#include "maru/c/ext/vulkan.h"
#include "maru_internal.h"
#include "ext_vulkan_internal.h"
#include "vulkan_api_constraints.h"

#ifdef MARU_INDIRECT_BACKEND

extern MARU_Status maru_vulkan_enable_wayland(MARU_Context *context, MARU_VkGetInstanceProcAddrFunc vk_loader);
extern MARU_Status maru_vulkan_enable_x11(MARU_Context *context, MARU_VkGetInstanceProcAddrFunc vk_loader);

#if defined(_WIN32)
    __declspec(allocate(".maru_ext$a")) extern const void* const _maru_ext_start_marker;
#else
    extern const MARU_ExtensionDescriptor* const __start_maru_ext_hooks;
#endif

// This descriptor pointer is placed in the linker section.
static void _maru_vulkan_auto_init(MARU_Context *ctx, MARU_ExtensionID id);
MARU_REGISTER_EXTENSION("vulkan", _maru_vulkan_auto_init)

static MARU_ExtensionID _maru_vulkan_get_id(void) {
#if defined(_WIN32)
    return (MARU_ExtensionID)(&_maru_ext_ptr__maru_vulkan_auto_init - (&_maru_ext_start_marker + 1));
#else
    return (MARU_ExtensionID)(&_maru_ext_ptr__maru_vulkan_auto_init - &__start_maru_ext_hooks);
#endif
}

static void _maru_vulkan_auto_init(MARU_Context *ctx, MARU_ExtensionID id) {
    (void)id;
    maru_vulkan_enable(ctx, NULL);
}

MARU_API MARU_Status maru_vulkan_enable(MARU_Context *context, MARU_VkGetInstanceProcAddrFunc vk_loader) {
  MARU_VULKAN_API_VALIDATE(enable, context, vk_loader);

  MARU_BackendType type = maru_getContextBackendType(context);
  switch(type) {
    case MARU_BACKEND_WAYLAND:
      return maru_vulkan_enable_wayland(context, vk_loader);
    case MARU_BACKEND_X11:
      return maru_vulkan_enable_x11(context, vk_loader);
    default:
      return MARU_FAILURE;
  }
}

MARU_API const char **maru_vulkan_getVkExtensions(MARU_Context *context, uint32_t *out_count) {
  MARU_VULKAN_API_VALIDATE(getVkExtensions, context, out_count);
  
  MARU_ExtensionID id = _maru_vulkan_get_id();
  const MARU_ExtVulkanVTable* const* vtable_ptr = (const MARU_ExtVulkanVTable* const*)maru_getExtension(context, id);
  if (!vtable_ptr) {
      *out_count = 0;
      return NULL;
  }
  return (*vtable_ptr)->getVkExtensions(context, out_count);
}

MARU_API MARU_Status maru_vulkan_createVkSurface(MARU_Window *window, 
                                        VkInstance instance,
                                        VkSurfaceKHR *out_surface) {
  MARU_VULKAN_API_VALIDATE(createVkSurface, window, instance, out_surface);

  MARU_Context *context = maru_getWindowContext(window);
  MARU_ExtensionID id = _maru_vulkan_get_id();
  const MARU_ExtVulkanVTable* const* vtable_ptr = (const MARU_ExtVulkanVTable* const*)maru_getExtension(context, id);
  if (!vtable_ptr) return MARU_FAILURE;
  
  return (*vtable_ptr)->createVkSurface(window, instance, out_surface);
}
#endif
