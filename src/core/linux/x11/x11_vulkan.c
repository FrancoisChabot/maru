#include "maru/c/vulkan.h"
#include "vulkan_api_constraints.h"

const char **maru_getVkExtensions_X11(const MARU_Context *context,
                                      uint32_t *out_count) {
  (void)context;
  *out_count = 0;
  return NULL;
}

MARU_Status maru_createVkSurface_X11(MARU_Window *window, VkInstance instance,
                                     MARU_VkGetInstanceProcAddrFunc vk_loader,
                                     VkSurfaceKHR *out_surface) {
  (void)window;
  (void)instance;
  (void)vk_loader;
  (void)out_surface;
  return MARU_FAILURE;
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
