#include "wayland_internal.h"
#include "maru_api_constraints.h"

#ifdef MARU_ENABLE_VULKAN
extern MARU_Status maru_getVkExtensions_WL(MARU_Context *context, MARU_ExtensionList *out_list);
extern MARU_Status maru_createVkSurface_WL(MARU_Window *window, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface);
#endif

#ifdef MARU_INDIRECT_BACKEND
const MARU_Backend maru_backend_WL = {
  .destroyContext = maru_destroyContext_WL,
  .pumpEvents = maru_pumpEvents_WL,
  .createWindow = maru_createWindow_WL,
  .destroyWindow = maru_destroyWindow_WL,
  .getWindowGeometry = maru_getWindowGeometry_WL,
#ifdef MARU_ENABLE_VULKAN
  .getVkExtensions = maru_getVkExtensions_WL,
  .createVkSurface = maru_createVkSurface_WL,
#endif
};
#else
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_WL(create_info, out_context);
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  return maru_destroyContext_WL(context);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms);
  return maru_pumpEvents_WL(context, timeout_ms);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  return maru_createWindow_WL(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return maru_destroyWindow_WL(window);
}

MARU_API MARU_Status maru_getWindowGeometry(MARU_Window *window,
                                              MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  return maru_getWindowGeometry_WL(window, out_geometry);
}
#endif
