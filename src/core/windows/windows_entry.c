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
  .wakeContext = maru_wakeContext_Windows,
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

MARU_API MARU_Status maru_wakeContext(MARU_Context *context) {
  MARU_API_VALIDATE(wakeContext, context);
  return maru_wakeContext_Windows(context);
}
#endif
