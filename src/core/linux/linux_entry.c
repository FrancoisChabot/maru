#include "maru_api_constraints.h"
#include <stdlib.h>

#ifdef MARU_INDIRECT_BACKEND

// These two are special-cased because they are the ones seting up the vtable.
extern MARU_Status
maru_createContext_WL(const MARU_ContextCreateInfo *create_info,
                      MARU_Context **out_context);
extern MARU_Status
maru_createContext_X11(const MARU_ContextCreateInfo *create_info,
                       MARU_Context **out_context);

MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                               MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  MARU_BackendType backend = create_info->backend;

  if (backend == MARU_BACKEND_WAYLAND || backend == MARU_BACKEND_UNKNOWN) {
    if (maru_createContext_WL(create_info, out_context) == MARU_SUCCESS) {
      return MARU_SUCCESS;
    }
  }

  if (backend == MARU_BACKEND_X11 || backend == MARU_BACKEND_UNKNOWN) {
    if (maru_createContext_X11(create_info, out_context) == MARU_SUCCESS) {
      return MARU_SUCCESS;
    }
  }

  return MARU_FAILURE;
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->destroyContext(context);
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->pumpEvents(context, timeout_ms);
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
  return ctx_base->backend->createWindow(context, create_info, out_window);
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
  return win_base->backend->destroyWindow(window);
}

#endif
