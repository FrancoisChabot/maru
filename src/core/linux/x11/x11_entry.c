#include "maru_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"
#include "maru/c/cursors.h"
#include "maru/c/monitors.h"
#include <stdlib.h>
#include <string.h>

MARU_Status maru_createContext_X11(const MARU_ContextCreateInfo *create_info,
                                   MARU_Context **out_context) {
  MARU_Context_Base *ctx = (MARU_Context_Base *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_Base));
  if (!ctx) {
    return MARU_FAILURE;
  }

  if (create_info->allocator.alloc_cb) {
    ctx->allocator = create_info->allocator;
  } else {
    ctx->allocator.alloc_cb = _maru_default_alloc;
    ctx->allocator.realloc_cb = _maru_default_realloc;
    ctx->allocator.free_cb = _maru_default_free;
    ctx->allocator.userdata = NULL;
  }

  ctx->pub.userdata = create_info->userdata;
  ctx->pub.flags = MARU_CONTEXT_STATE_READY;
  ctx->diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->diagnostic_userdata = create_info->attributes.diagnostic_userdata;

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_X11;
  ctx->backend = &maru_backend_X11;
#endif

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx->creator_thread = _maru_getCurrentThreadId();
#endif

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_X11(MARU_Context *context) {
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  maru_context_free(ctx_base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_pumpEvents_X11(MARU_Context *context, uint32_t timeout_ms) {
  (void)context;
  (void)timeout_ms;
  return MARU_SUCCESS;
}

MARU_Status maru_createWindow_X11(MARU_Context *context,
                                 const MARU_WindowCreateInfo *create_info,
                                 MARU_Window **out_window) {
  (void)context;
  (void)create_info;
  (void)out_window;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_X11(MARU_Window *window) {
  (void)window;
  return MARU_SUCCESS;
}

#ifdef MARU_ENABLE_VULKAN
extern MARU_Status maru_getVkExtensions_X11(MARU_Context *context, MARU_ExtensionList *out_list);
extern MARU_Status maru_createVkSurface_X11(MARU_Window *window, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface);
#endif

#ifdef MARU_INDIRECT_BACKEND
const MARU_Backend maru_backend_X11 = {
  .destroyContext = maru_destroyContext_X11,
  .pumpEvents = maru_pumpEvents_X11,
  .createWindow = maru_createWindow_X11,
  .destroyWindow = maru_destroyWindow_X11,
  .getWindowGeometry = NULL,
  .updateWindow = NULL,
  .requestWindowFocus = NULL,
  .getWindowBackendHandle = NULL,
  .getStandardCursor = NULL,
  .createCursor = NULL,
  .destroyCursor = NULL,
  .wakeContext = NULL,
  .updateMonitors = NULL,
  .destroyMonitor = NULL,
  .getMonitorModes = NULL,
  .setMonitorMode = NULL,
  .resetMonitorMetrics = NULL,
#ifdef MARU_ENABLE_VULKAN
  .getVkExtensions = maru_getVkExtensions_X11,
  .createVkSurface = maru_createVkSurface_X11,
#endif
};
#else
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_X11(create_info, out_context);
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  return maru_destroyContext_X11(context);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms);
  return maru_pumpEvents_X11(context, timeout_ms);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  return maru_createWindow_X11(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return maru_destroyWindow_X11(window);
}

MARU_API MARU_Status maru_getWindowGeometry(MARU_Window *window,
                                              MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  return MARU_FAILURE; // Not implemented in X11 yet
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  MARU_Window_Base *win_base = (MARU_Window_Base *)window;
  _maru_update_window_base(win_base, field_mask, attributes);
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_getStandardCursor(MARU_Context *context, MARU_CursorShape shape,
                                             MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(getStandardCursor, context, shape, out_cursor);
  return MARU_FAILURE; // Not implemented in X11 yet
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  return MARU_FAILURE; // Not implemented in X11 yet
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  return MARU_FAILURE; // Not implemented in X11 yet
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


