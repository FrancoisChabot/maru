#include "maru_internal.h"
#include "maru_mem_internal.h"

MARU_Status maru_createContext_X11(const MARU_ContextCreateInfo *create_info,
                                   MARU_Context **out_context) {
  MARU_Context_Base *ctx = (MARU_Context_Base *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_Base));
  if (!ctx) {
    return MARU_FAILURE;
  }

  if (create_info->allocator.alloc_cb) {
    ctx->allocator = create_info->allocator;
  } else {
    ctx->allocator.alloc_cb = _maru_default_alloc;
    ctx->allocator.realloc_cb = _maru_default_realloc;
    ctx->allocator.free_cb = _maru_default_free;
    ctx->allocator.userdata = NULL;
  }

  ctx->pub.userdata = create_info->userdata;
  ctx->pub.flags = MARU_CONTEXT_STATE_READY;
  ctx->diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->diagnostic_userdata = create_info->attributes.diagnostic_userdata;

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_X11;
  ctx->backend = &maru_backend_X11;
#endif

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx->creator_thread = _maru_getCurrentThreadId();
#endif

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_X11(MARU_Context *context) {
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
  maru_context_free(ctx_base, context);
  return MARU_SUCCESS;
}

#ifdef MARU_INDIRECT_BACKEND
const MARU_Backend maru_backend_X11 = {.destroyContext = maru_destroyContext_X11};
#else
MARU_API MARU_Status maru_createContext(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  return maru_createContext_X11(create_info, out_context);
}

MARU_API MARU_Status maru_destroyContext(MARU_Context *context) {
  return maru_destroyContext_X11(context);
}
#endif
