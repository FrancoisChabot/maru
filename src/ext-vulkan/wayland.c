#include "maru/c/ext/vulkan.h"
#include "maru_internal.h"
#include "maru_api_constraints.h"

#ifdef MARU_ENABLE_VULKAN

MARU_Status maru_getVkExtensions_WL(MARU_Context *context, MARU_ExtensionList *out_list) {
  (void)context;
  (void)out_list;
  return MARU_FAILURE;
}

MARU_Status maru_createVkSurface_WL(MARU_Window *window, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface) {
  (void)window;
  (void)instance;
  (void)out_surface;
  return MARU_FAILURE;
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
