#include "ext_vulkan_internal.h"
#include "maru/c/core.h"
#include "maru/c/ext/vulkan.h"
#include "maru_api_constraints.h"
#include "maru_internal.h"
#include "vulkan_api_constraints.h"
#include <string.h>

typedef struct MARU_VulkanExtContextState_X11 {
#ifdef MARU_INDIRECT_BACKEND
  const MARU_ExtVulkanVTable *vtable; // MUST BE THE FIRST MEMBER!
#else
  char _unused;
#endif

} MARU_VulkanExtContextState_X11;

static const char **_getVkExtensions_X11(const MARU_Context *context, uint32_t *out_count) {
  (void)context;
  (void)out_count;
  return NULL;
}

static MARU_Status
_createVkSurface_X11(MARU_Window *window, VkInstance instance,
                     MARU_VkGetInstanceProcAddrFunc vk_loader,
                     VkSurfaceKHR *out_surface) {
  (void)window;
  (void)instance;
  (void)vk_loader;
  (void)out_surface;
  return MARU_FAILURE;
}

#ifdef MARU_INDIRECT_BACKEND
static const MARU_ExtVulkanVTable maru_ext_vk_backend_X11 = {
    .getVkExtensions = _getVkExtensions_X11,
    .createVkSurface = _createVkSurface_X11,
};
#endif

static void _maru_vulkan_cleanup_X11(MARU_Context *context, void *state) {
  maru_contextFree(context, state);
}

MARU_Status maru_vulkan_init_x11(MARU_Context *context) {
  MARU_VulkanExtContextState_X11 *state = (MARU_VulkanExtContextState_X11 *)maru_getExtension(context, MARU_EXT_VULKAN);
  if (!state) {
    state = (MARU_VulkanExtContextState_X11 *)maru_contextAlloc(
        context, sizeof(MARU_VulkanExtContextState_X11));
    if (!state)
      return MARU_FAILURE;
    memset(state, 0, sizeof(MARU_VulkanExtContextState_X11));

#ifdef MARU_INDIRECT_BACKEND
    state->vtable = &maru_ext_vk_backend_X11;
#endif
    maru_registerExtension(context, MARU_EXT_VULKAN, state,
                           _maru_vulkan_cleanup_X11);
  }

  return MARU_SUCCESS;
}

#ifndef MARU_INDIRECT_BACKEND
MARU_API const char **maru_vulkan_getVkExtensions(const MARU_Context *context, uint32_t *out_count) {
  MARU_VULKAN_API_VALIDATE(getVkExtensions, context, out_count);
  return _getVkExtensions_X11(context, out_count);
}

MARU_API MARU_Status maru_vulkan_createVkSurface(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface) {
  MARU_VULKAN_API_VALIDATE(createVkSurface, window, instance, vk_loader,
                           out_surface);
  return _createVkSurface_X11(window, instance, vk_loader, out_surface);
}
#endif
