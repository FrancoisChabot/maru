#include "maru_internal.h"
#include "maru_api_constraints.h"
#include <stdlib.h>

MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                               MARU_Context **out_context) {
  MARU_API_VALIDATE(createContext, create_info, out_context);
  MARU_Context_Base *ctx =
      (MARU_Context_Base *)malloc(sizeof(MARU_Context_Base));
  if (!ctx)
    return MARU_FAILURE;

  ctx->pub.userdata = create_info->userdata;
  ctx->pub.flags = MARU_CONTEXT_STATE_READY;
  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  MARU_API_VALIDATE(destroyContext, context);
  free(context);
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_pumpEvents(MARU_Context *context, uint32_t timeout_ms) {
  MARU_API_VALIDATE(pumpEvents, context, timeout_ms);
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_createWindow(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_API_VALIDATE(createWindow, context, create_info, out_window);
  (void)out_window;
  return MARU_SUCCESS;
}

MARU_API MARU_Status maru_destroyWindow(MARU_Window *window) {
  MARU_API_VALIDATE(destroyWindow, window);
  return MARU_SUCCESS;
}
