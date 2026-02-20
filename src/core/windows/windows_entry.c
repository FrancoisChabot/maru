#include "windows_internal.h"
#include "maru_api_constraints.h"

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
  .getWindowBackendHandle = maru_getWindowBackendHandle_Windows,
  .getStandardCursor = maru_getStandardCursor_Windows,
  .createCursor = maru_createCursor_Windows,
  .destroyCursor = maru_destroyCursor_Windows,
  .wakeContext = maru_wakeContext_Windows,
  .updateMonitors = maru_updateMonitors_Windows,
  .destroyMonitor = maru_destroyMonitor_Windows,
  .getMonitorModes = maru_getMonitorModes_Windows,
  .setMonitorMode = maru_setMonitorMode_Windows,
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

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms);
  return maru_pumpEvents_Windows(context, timeout_ms);
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

MARU_API MARU_Status maru_getWindowGeometry(MARU_Window *window,
                                             MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  return maru_getWindowGeometry_Windows(window, out_geometry);
}

MARU_API MARU_Status maru_requestWindowFocus(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowFocus, window);
  return maru_requestWindowFocus_Windows(window);
}

MARU_API MARU_Status maru_getWindowBackendHandle(MARU_Window *window,
                                               MARU_BackendType *out_type,
                                               MARU_BackendHandle *out_handle) {
  MARU_API_VALIDATE(getWindowBackendHandle, window, out_type, out_handle);
  return maru_getWindowBackendHandle_Windows(window, out_type, out_handle);
}

MARU_API MARU_Status maru_getStandardCursor(MARU_Context *context, MARU_CursorShape shape,
                                              MARU_Cursor **out_cursor) {
  MARU_API_VALIDATE(getStandardCursor, context, shape, out_cursor);
  return maru_getStandardCursor_Windows(context, shape, out_cursor);
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

MARU_API MARU_Status maru_resetCursorMetrics(MARU_Cursor *cursor) {
  MARU_API_VALIDATE(resetCursorMetrics, cursor);
  MARU_Cursor_Base *cur_base = (MARU_Cursor_Base *)cursor;
  memset(&cur_base->metrics, 0, sizeof(MARU_CursorMetrics));
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_Windows(context);
}

MARU_API MARU_Status maru_getMonitors(MARU_Context *context, MARU_MonitorList *out_list) {
  MARU_API_VALIDATE(getMonitors, context, out_list);
  MARU_Status res = maru_updateMonitors_Windows(context);
  if (res != MARU_SUCCESS) return res;
  
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  out_list->monitors = ctx_base->monitor_cache;
  out_list->count = ctx_base->monitor_cache_count;
  return MARU_SUCCESS;
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
    if (mon_base->ref_count == 0 && !mon_base->is_active) {
      _maru_monitor_free(mon_base);
    }
  }
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_getMonitorModes(const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
  MARU_API_VALIDATE(getMonitorModes, monitor, out_list);
  return maru_getMonitorModes_Windows(monitor, out_list);
}

MARU_API MARU_Status maru_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
  MARU_API_VALIDATE(setMonitorMode, monitor, mode);
  return maru_setMonitorMode_Windows(monitor, mode);
}

MARU_API MARU_Status maru_resetMonitorMetrics(MARU_Monitor *monitor) {
  MARU_API_VALIDATE(resetMonitorMetrics, monitor);
  MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)monitor;
  memset(&mon_base->metrics, 0, sizeof(MARU_MonitorMetrics));
  return MARU_SUCCESS;
}
#endif
