#include "windows_internal.h"
#include "maru_api_constraints.h"
#include "maru/c/native/win32.h"
#include <stdatomic.h>

extern void *_maru_getContextNativeHandle_Windows(MARU_Context *context);
extern void *_maru_getWindowNativeHandle_Windows(MARU_Window *window);
extern const char **maru_getVkExtensions_Windows(const MARU_Context *context,
                                                 uint32_t *out_count);
extern MARU_Status maru_createVkSurface_Windows(
    MARU_Window *window, VkInstance instance,
    MARU_VkGetInstanceProcAddrFunc vk_loader, VkSurfaceKHR *out_surface);

#ifdef MARU_INDIRECT_BACKEND
const MARU_Backend maru_backend_Windows = {
  .destroyContext = maru_destroyContext_Windows,
  .pumpEvents = maru_pumpEvents_Windows,
  .updateContext = maru_updateContext_Windows,
  .createWindow = maru_createWindow_Windows,
  .destroyWindow = maru_destroyWindow_Windows,
  .getWindowGeometry = maru_getWindowGeometry_Windows,
  .updateWindow = maru_updateWindow_Windows,
  .requestWindowFocus = maru_requestWindowFocus_Windows,
  .requestWindowAttention = maru_requestWindowAttention_Windows,
  .createCursor = maru_createCursor_Windows,
  .destroyCursor = maru_destroyCursor_Windows,
  .createImage = maru_createImage_Windows,
  .destroyImage = maru_destroyImage_Windows,
  .wakeContext = maru_wakeContext_Windows,
  .updateMonitors = maru_updateMonitors_Windows,
  .getMonitorModes = maru_getMonitorModes_Windows,
  .setMonitorMode = maru_setMonitorMode_Windows,
  .getContextNativeHandle = _maru_getContextNativeHandle_Windows,
  .getWindowNativeHandle = _maru_getWindowNativeHandle_Windows,
  .getVkExtensions = maru_getVkExtensions_Windows,
  .createVkSurface = maru_createVkSurface_Windows,
};
#else
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                         MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_Windows(create_info, out_context);
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  return maru_destroyContext_Windows(context);
}

MARU_API MARU_Status maru_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  return maru_updateContext_Windows(context, field_mask, attributes);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, callback, userdata);
  return maru_pumpEvents_Windows(context, timeout_ms, callback, userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                        const MARU_WindowCreateInfo *create_info,
                                        MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  return maru_createWindow_Windows(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return maru_destroyWindow_Windows(window);
}

MARU_API MARU_Status maru_updateWindow(MARU_Window *window, uint64_t field_mask,
                                         const MARU_WindowAttributes *attributes) {
  MARU_API_VALIDATE(updateWindow, window, field_mask, attributes);
  return maru_updateWindow_Windows(window, field_mask, attributes);
}

MARU_API void maru_getWindowGeometry(MARU_Window *window,
                                             MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  maru_getWindowGeometry_Windows(window, out_geometry);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  return maru_requestWindowFocus_Windows(window);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  return maru_requestWindowAttention_Windows(window);
}

MARU_API MARU_Status maru_createCursor(MARU_Context *context,
                                         const MARU_CursorCreateInfo *create_info,
                                         MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(createCursor, context, create_info, out_cursor);
  return maru_createCursor_Windows(context, create_info, out_cursor);
}

MARU_API MARU_Status maru_destroyCursor(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(destroyCursor, cursor);
  return maru_destroyCursor_Windows(cursor);
}

MARU_API MARU_Status maru_createImage(MARU_Context *context,
                                      const MARU_ImageCreateInfo *create_info,
                                      MARU_Image **out_image) {
  MARU_API_VALIDATE(createImage, context, create_info, out_image);
  return maru_createImage_Windows(context, create_info, out_image);
}

MARU_API MARU_Status maru_destroyImage(MARU_Image *image) {
  MARU_API_VALIDATE(destroyImage, image);
  return maru_destroyImage_Windows(image);
}

MARU_API void maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  memset(&cur_base->metrics, 0, sizeof(MARU_CursorMetrics));
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_Windows(context);
}

MARU_API MARU_Monitor *const *maru_getMonitors(MARU_Context *context, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitors, context, out_count);
  MARU_Status res = maru_updateMonitors_Windows(context);
  if (res != MARU_SUCCESS) {
    *out_count = 0;
    return NULL;
  }
  
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  *out_count = ctx_base->monitor_cache_count;
  return ctx_base->monitor_cache;
}

MARU_API void maru_retainMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(retainMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  atomic_fetch_add_explicit(&mon_base->ref_count, 1u, memory_order_relaxed);
}

MARU_API void maru_releaseMonitor(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(releaseMonitor, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  uint32_t current = atomic_load_explicit(&mon_base->ref_count, memory_order_acquire);
  while (current > 0) {
    if (atomic_compare_exchange_weak_explicit(&mon_base->ref_count, &current,
                                              current - 1u, memory_order_acq_rel,
                                              memory_order_acquire)) {
      if (current == 1u && !mon_base->is_active) {
        _maru_monitor_free(mon_base);
      }
      return;
    }
  }
}

MARU_API const MARU_VideoMode *maru_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_count);
  return maru_getMonitorModes_Windows(monitor, out_count);
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_Windows(monitor, mode);
}

MARU_API void maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  memset(&mon_base->metrics, 0, sizeof(MARU_MonitorMetrics));
}

MARU_API MARU_Status maru_getWin32ContextHandle(
    MARU_Context *context, MARU_Win32ContextHandle *out_handle) {
  if (!context || !out_handle) return MARU_FAILURE;
  out_handle->instance = (HINSTANCE)_maru_getContextNativeHandle_Windows(context);
  return out_handle->instance ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getWin32WindowHandle(
    MARU_Window *window, MARU_Win32WindowHandle *out_handle) {
  if (!window || !out_handle) return MARU_FAILURE;
  MARU_Context *context = maru_getWindowContext(window);
  out_handle->instance = (HINSTANCE)_maru_getContextNativeHandle_Windows(context);
  out_handle->hwnd = (HWND)_maru_getWindowNativeHandle_Windows(window);
  return (out_handle->instance && out_handle->hwnd) ? MARU_SUCCESS : MARU_FAILURE;
}
#endif
