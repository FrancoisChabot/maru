#include "maru_api_constraints.h"
#include <string.h>

#include <stdlib.h>

#include "maru/c/instrumentation.h"

#ifdef MARU_VALIDATE_API_CALLS
#include <pthread.h>
#include <stdio.h>

MARU_ThreadId _maru_getCurrentThreadId(void) {
  return (MARU_ThreadId)pthread_self();
}

void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg) {
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)ctx;
  if (ctx_base && ctx_base->diagnostic_cb) {
    MARU_DiagnosticInfo info = {
        .diagnostic = diag,
        .message = msg,
        .context = (MARU_Context *)ctx,
        .window = NULL
    };
    ctx_base->diagnostic_cb(&info, ctx_base->diagnostic_userdata);
  } else {
    fprintf(stderr, "[MARU DIAGNOSTIC %d] %s\n", (int)diag, msg);
  }
}
#else
void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg) {
  const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)ctx;
  if (ctx_base && ctx_base->diagnostic_cb) {
    MARU_DiagnosticInfo info = {
        .diagnostic = diag,
        .message = msg,
        .context = (MARU_Context *)ctx,
        .window = NULL
    };
    ctx_base->diagnostic_cb(&info, ctx_base->diagnostic_userdata);
  }
}
#endif

MARU_API MARU_Status maru_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  MARU_API_VALIDATE(updateContext, context, field_mask, attributes);
  MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;

  if (field_mask & MARU_CONTEXT_ATTR_DIAGNOSTICS) {
    ctx_base->diagnostic_cb = attributes->diagnostic_cb;
    ctx_base->diagnostic_userdata = attributes->diagnostic_userdata;
  }

  // Other attributes like event mask/callback will be implemented when we have events
  return MARU_SUCCESS;
}

void *_maru_default_alloc(size_t size, void *userdata) {
  (void)userdata;
  return malloc(size);
}

void *_maru_default_realloc(void *ptr, size_t new_size, void *userdata) {
  (void)userdata;
  return realloc(ptr, new_size);
}

void _maru_default_free(void *ptr, void *userdata) {
  (void)userdata;
  free(ptr);
}

MARU_API MARU_Version maru_getVersion(void) {
  return (MARU_Version){
      .major = MARU_VERSION_MAJOR,
      .minor = MARU_VERSION_MINOR,
      .patch = MARU_VERSION_PATCH
  };
}

