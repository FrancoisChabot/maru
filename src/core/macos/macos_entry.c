#include "macos_internal.h"
#include "maru_api_constraints.h"

static MARU_Status maru_requestWindowAttention_Cocoa(MARU_Window *window) {
  (void)window;
  return MARU_FAILURE;
}

#ifdef MARU_INDIRECT_BACKEND
const MARU_Backend maru_backend_Cocoa = {
  .destroyContext = maru_destroyContext_Cocoa,
  .pumpEvents = maru_pumpEvents_Cocoa,
  .createWindow = maru_createWindow_Cocoa,
  .destroyWindow = maru_destroyWindow_Cocoa,
  .getWindowGeometry = maru_getWindowGeometry_Cocoa,
  .requestWindowAttention = maru_requestWindowAttention_Cocoa,
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
#endif
