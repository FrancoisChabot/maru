#include "macos_internal.h"
#include "maru_api_constraints.h"
#include "maru/c/native/cocoa.h"

static MARU_Status maru_requestWindowAttention_Cocoa(MARU_Window *window) {
  (void)window;
  return MARU_FAILURE;
}

static void *maru_getContextNativeHandle_Cocoa(MARU_Context *context) {
  MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)context;
  return ctx->ns_app;
}

static void *maru_getWindowNativeHandle_Cocoa(MARU_Window *window) {
  MARU_Window_Cocoa *win = (MARU_Window_Cocoa *)window;
  return win->ns_window;
}

#ifdef MARU_INDIRECT_BACKEND
const MARU_Backend maru_backend_Cocoa = {
  .destroyContext = maru_destroyContext_Cocoa,
  .pumpEvents = maru_pumpEvents_Cocoa,
  .createWindow = maru_createWindow_Cocoa,
  .destroyWindow = maru_destroyWindow_Cocoa,
  .getWindowGeometry = maru_getWindowGeometry_Cocoa,
  .requestWindowAttention = maru_requestWindowAttention_Cocoa,
  .getContextNativeHandle = maru_getContextNativeHandle_Cocoa,
  .getWindowNativeHandle = maru_getWindowNativeHandle_Cocoa,
  .getVkExtensions = NULL,
  .createVkSurface = NULL,
};
#else
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                         MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  return maru_createContext_Cocoa(create_info, out_context);
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  return maru_destroyContext_Cocoa(context);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms, callback, userdata);
  return maru_pumpEvents_Cocoa(context, timeout_ms, callback, userdata);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                        const MARU_WindowCreateInfo *create_info,
                                        MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  return maru_createWindow_Cocoa(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return maru_destroyWindow_Cocoa(window);
}

MARU_API void maru_getWindowGeometry(MARU_Window *window,
                                             MARU_WindowGeometry *out_geometry) {
  MARU_API_VALIDATE(getWindowGeometry, window, out_geometry);
  maru_getWindowGeometry_Cocoa(window, out_geometry);
}

MARU_API MARU_Status maru_requestWindowAttention(MARU_Window *window) {
  MARU_API_VALIDATE(requestWindowAttention, window);
  return maru_requestWindowAttention_Cocoa(window);
}

MARU_API MARU_Status maru_getCocoaContextHandle(
    MARU_Context *context, MARU_CocoaContextHandle *out_handle) {
  if (!context || !out_handle) return MARU_FAILURE;
  out_handle->ns_application = maru_getContextNativeHandle_Cocoa(context);
  return out_handle->ns_application ? MARU_SUCCESS : MARU_FAILURE;
}

MARU_API MARU_Status maru_getCocoaWindowHandle(
    MARU_Window *window, MARU_CocoaWindowHandle *out_handle) {
  if (!window || !out_handle) return MARU_FAILURE;
  out_handle->ns_window = maru_getWindowNativeHandle_Cocoa(window);
  return out_handle->ns_window ? MARU_SUCCESS : MARU_FAILURE;
}
#endif
