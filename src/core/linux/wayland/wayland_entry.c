#include "wayland_internal.h"
#include "maru_api_constraints.h"
#include "maru/c/cursors.h"
#include "maru/c/monitors.h"
#include <stdlib.h>
#include <string.h>

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
  .updateWindow = maru_updateWindow_WL,
  .requestWindowFocus = NULL,
  .getWindowBackendHandle = NULL,
  .getStandardCursor = maru_getStandardCursor_WL,
  .createCursor = maru_createCursor_WL,
  .destroyCursor = maru_destroyCursor_WL,
  .wakeContext = NULL,
  .updateMonitors = NULL,
  .destroyMonitor = NULL,
  .getMonitorModes = NULL,
  .setMonitorMode = NULL,
  .resetMonitorMetrics = NULL,
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

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                         const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  return maru_updateWindow_WL(window, field_mask, attributes);
}

MARU_API MARU_Status maru_getStandardCursor(MARU_Context *context, MARU_CursorShape shape,
                                              MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(getStandardCursor, context, shape, out_cursor);
  return maru_getStandardCursor_WL(context, shape, out_cursor);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                        const MARU_CursorCreateInfo *create_info,
                                        MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  return maru_createCursor_WL(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  return maru_destroyCursor_WL(cursor);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_getWindowBackendHandle(MARU_Window *window,
                                               MARU_BackendType *out_type,
                                               MARU_BackendHandle *out_handle) {
  MARU_API_VALIDATE(getWindowBackendHandle, window, out_type, out_handle);
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_getMonitors(MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  mon_base->ref_count++;
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  if (mon_base->ref_count > 0) {
    mon_base->ref_count--;
  }
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_getMonitorModes(const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_list);
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  return MARU_FAILURE;
}

MARU_API MARU_Status maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  memset(&mon_base->metrics, 0, sizeof(MARU_MonitorMetrics));
  return MARU_SUCCESS;
}
#endif
