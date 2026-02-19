#include "maru_internal.h"

MARU_Status maru_createContext_X11(const MARU_ContextCreateInfo *create_info,
                                   MARU_Context **out_context) {
  // Real implementation would probe X11 here
  MARU_Context_Base *ctx =
      (MARU_Context_Base *)malloc(sizeof(MARU_Context_Base));
  if (!ctx)
    return MARU_FAILURE;

  ctx->pub.userdata = create_info->userdata;
  ctx->pub.flags = MARU_CONTEXT_STATE_READY;
#ifdef MARU_INDIRECT_BACKEND
  ctx->backend = &x11_backend;
#endif

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_X11(MARU_Context *context) {
  free(context);
  return MARU_SUCCESS;
}