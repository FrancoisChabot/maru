#define _GNU_SOURCE
#include "maru_internal.h"
#include <dlfcn.h>

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

  for (uint32_t i = 0; i < create_info->extension_count; ++i) {
    if (create_info->extensions[i]) {
      MARU_ExtensionInitFunc init_fn;
      *(const void **)(&init_fn) = create_info->extensions[i];
      (void)init_fn(*out_context);
    }
  }

  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_X11(MARU_Context *context) {
  free(context);
  return MARU_SUCCESS;
}