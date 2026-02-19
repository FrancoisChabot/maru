#include "maru_internal.h"
#include <stdlib.h>

MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                               MARU_Context **out_context) {
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
  free(context);
  return MARU_SUCCESS;
}
