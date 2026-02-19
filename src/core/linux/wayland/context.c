MARU_Status maru_createContext_WL(const MARU_ContextCreateInfo *create_info,
                                  MARU_Context **out_context) {
  // In a real implementation, we would probe if Wayland is available here
  MARU_Context_Base *ctx =
      (MARU_Context_Base *)malloc(sizeof(MARU_Context_Base));
  if (!ctx)
    return MARU_FAILURE;

  ctx->pub.userdata = create_info->userdata;
  ctx->pub.flags = MARU_CONTEXT_STATE_READY;
#ifdef MARU_INDIRECT_BACKEND
  ctx->backend = &wl_backend;
#endif

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_WL(MARU_Context *context) {
  free(context);
  return MARU_SUCCESS;
}
