#include "maru/c/core.h"
#include "maru/c/ext/vulkan.h"
#include "maru/c/details/contexts.h"
#include "maru_internal.h"
#include "ext_vulkan_internal.h"
#include "vulkan_api_constraints.h"

#ifdef MARU_INDIRECT_BACKEND

extern MARU_Status maru_vulkan_init_wayland(MARU_Context *context);
extern MARU_Status maru_vulkan_init_x11(MARU_Context *context);

MARU_API MARU_Status maru_vulkan_init(MARU_Context *context) {
  MARU_BackendType type = maru_getContextBackend(context);
  switch(type) {
    case MARU_BACKEND_WAYLAND:
      return maru_vulkan_init_wayland(context);
    case MARU_BACKEND_X11:
      return maru_vulkan_init_x11(context);
    default:
      return MARU_FAILURE;
  }
}

MARU_API const char **maru_vulkan_getVkExtensions(const MARU_Context *context, uint32_t *out_count) {
  MARU_VULKAN_API_VALIDATE(getVkExtensions, context, out_count);
  
  const MARU_ExtVulkanVTable* const* vtable_ptr = (const MARU_ExtVulkanVTable* const*)maru_getExtension(context, MARU_EXT_VULKAN);
  if (!vtable_ptr) {
      *out_count = 0;
      return NULL;
  }
  return (*vtable_ptr)->getVkExtensions(context, out_count);
}

MARU_API MARU_Status maru_vulkan_createVkSurface(MARU_Window *window, 
                                        VkInstance instance,
                                        MARU_VkGetInstanceProcAddrFunc vk_loader,
                                        VkSurfaceKHR *out_surface) {
  MARU_VULKAN_API_VALIDATE(createVkSurface, window, instance, vk_loader, out_surface);

  MARU_Context *context = maru_getWindowContext(window);
  const MARU_ExtVulkanVTable* const* vtable_ptr = (const MARU_ExtVulkanVTable* const*)maru_getExtension(context, MARU_EXT_VULKAN);
  if (!vtable_ptr) return MARU_FAILURE;
  
  return (*vtable_ptr)->createVkSurface(window, instance, vk_loader, out_surface);
}
#endif
