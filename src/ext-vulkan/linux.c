#include "maru/c/ext/vulkan.h"
#include "maru_internal.h"
#include "maru_api_constraints.h"

#ifdef MARU_ENABLE_VULKAN
#ifdef MARU_INDIRECT_BACKEND

MARU_API MARU_Status maru_getVkExtensions(MARU_Context *context, MARU_ExtensionList *out_list) {
  MARU_API_VALIDATE(getVkExtensions, context, out_list);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->getVkExtensions(context, out_list);
}

MARU_API MARU_Status maru_createVkSurface(MARU_Window *window, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface) {
  MARU_API_VALIDATE(createVkSurface, window, instance, out_surface);
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  return win_base->backend->createVkSurface(window, instance, out_surface);
}

#endif // MARU_INDIRECT_BACKEND
#endif // MARU_ENABLE_VULKAN
