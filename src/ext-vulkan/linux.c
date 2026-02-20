#include "maru/c/core.h"
#include "maru/c/ext/vulkan.h"
#include "maru_internal.h"
#include "ext_vulkan_internal.h"
#include "vulkan_api_constraints.h"

#ifdef MARU_INDIRECT_BACKEND

extern MARU_Status maru_vulkan_enable_wayland(MARU_Context *context, MARU_VkGetInstanceProcAddrFunc vk_loader);
extern MARU_Status maru_vulkan_enable_x11(MARU_Context *context, MARU_VkGetInstanceProcAddrFunc vk_loader);

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
  
  // The VTable pointer is guaranteed to be the first member of the state struct.
  const MARU_ExtVulkanVTable* const* vtable_ptr = (const MARU_ExtVulkanVTable* const*)maru_getExtension(context, MARU_EXT_VULKAN);
  return (*vtable_ptr)->getVkExtensions(context, out_count);
}

MARU_API MARU_Status maru_vulkan_createVkSurface(MARU_Window *window, 
                                        VkInstance instance,
                                        VkSurfaceKHR *out_surface) {
  MARU_VULKAN_API_VALIDATE(createVkSurface, window, instance, out_surface);

  MARU_Context *context = maru_getWindowContext(window);
  const MARU_ExtVulkanVTable* const* vtable_ptr = (const MARU_ExtVulkanVTable* const*)maru_getExtension(context, MARU_EXT_VULKAN);
  
  return (*vtable_ptr)->createVkSurface(window, instance, out_surface);
}
#endif
