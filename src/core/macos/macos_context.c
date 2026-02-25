// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "macos_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

static id _maru_cocoa_get_shared_application(void) {
  Class ns_app_class = objc_getClass("NSApplication");
  if (!ns_app_class) {
    return nil;
  }
  
  SEL shared_sel = sel_getUid("sharedApplication");
  if (!class_respondsToSelector(ns_app_class, shared_sel)) {
    return nil;
  }
  
  typedef id (*NSApplicationSharedType)(Class, SEL);
  return ((NSApplicationSharedType)objc_msgSend)(ns_app_class, shared_sel);
}

static void _maru_cocoa_process_events(MARU_Context_Cocoa *ctx, uint32_t timeout_ms) {
  (void)ctx;
  (void)timeout_ms;
}

MARU_Status maru_createContext_Cocoa(const MARU_ContextCreateInfo *create_info,
                                      MARU_Context **out_context) {
  MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_Cocoa));
  if (!ctx) {
    return MARU_FAILURE;
  }

  memset(((uint8_t*)ctx) + sizeof(MARU_Context_Base), 0, sizeof(MARU_Context_Cocoa) - sizeof(MARU_Context_Base));

  ctx->base.backend_type = MARU_BACKEND_COCOA;

  if (create_info->allocator.alloc_cb) {
    ctx->base.allocator = create_info->allocator;
  } else {
    ctx->base.allocator.alloc_cb = _maru_default_alloc;
    ctx->base.allocator.realloc_cb = _maru_default_realloc;
    ctx->base.allocator.free_cb = _maru_default_free;
    ctx->base.allocator.userdata = NULL;
  }

  ctx->base.pub.userdata = create_info->userdata;
  ctx->base.diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->base.diagnostic_userdata = create_info->attributes.diagnostic_userdata;
  ctx->base.event_mask = create_info->attributes.event_mask;

  if (create_info->tuning) {
    ctx->base.tuning = *create_info->tuning;
  } else {
    MARU_ContextTuning default_tuning = MARU_CONTEXT_TUNING_DEFAULT;
    ctx->base.tuning = default_tuning;
  }

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_Cocoa;
  ctx->base.backend = &maru_backend_Cocoa;
#endif

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx->base.creator_thread = _maru_getCurrentThreadId();
#endif

  ctx->ns_app = _maru_cocoa_get_shared_application();
  if (!ctx->ns_app) {
    MARU_REPORT_DIAGNOSTIC((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "Failed to obtain NSApplication");
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->base.pub.flags = MARU_CONTEXT_STATE_READY;
  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_Cocoa(MARU_Context *context) {
  MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)context;

  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_pumpEvents_Cocoa(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
  MARU_Context_Cocoa *ctx = (MARU_Context_Cocoa *)context;

  MARU_PumpContext pump_ctx = {
    .callback = callback,
    .userdata = userdata
  };
  ctx->base.pump_ctx = &pump_ctx;

  _maru_cocoa_process_events(ctx, timeout_ms);

  ctx->base.pump_ctx = NULL;
  return MARU_SUCCESS;
}
