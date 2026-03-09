// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <string.h>

MARU_Status maru_createContext_Windows(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_Windows));
  if (!ctx) {
    return MARU_FAILURE;
  }

  ctx->base.pub.backend_type = MARU_BACKEND_WINDOWS;

  if (create_info->allocator.alloc_cb) {
    ctx->base.allocator = create_info->allocator;
  } else {
    ctx->base.allocator.alloc_cb = _maru_default_alloc;
    ctx->base.allocator.realloc_cb = _maru_default_realloc;
    ctx->base.allocator.free_cb = _maru_default_free;
    ctx->base.allocator.userdata = NULL;
  }
  ctx->base.tuning = create_info->tuning;
  _maru_init_context_base(&ctx->base);

  ctx->instance = GetModuleHandleW(NULL);
  ctx->owner_thread_id = GetCurrentThreadId();
  ctx->wake_event = CreateEventW(NULL, FALSE, FALSE, NULL);

  WNDCLASSEXW wc = {0};
  wc.cbSize = sizeof(WNDCLASSEXW);
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = _maru_window_proc;
  wc.hInstance = ctx->instance;
  wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
  wc.lpszClassName = L"MARU_WindowClass";

  ctx->window_class = RegisterClassExW(&wc);
  if (!ctx->window_class) {
    if (ctx->wake_event) CloseHandle(ctx->wake_event);
    _maru_cleanup_context_base(&ctx->base);
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->base.pub.flags = MARU_CONTEXT_STATE_READY;
  ctx->base.attrs_requested = create_info->attributes;
  ctx->base.attrs_effective = create_info->attributes;
  ctx->base.attrs_dirty_mask = 0;
  ctx->base.diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->base.diagnostic_userdata = create_info->attributes.diagnostic_userdata;
  ctx->base.event_mask = create_info->attributes.event_mask;
  ctx->base.inhibit_idle = create_info->attributes.inhibit_idle;

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  while (ctx->base.window_list_head) {
    maru_destroyWindow_Windows((MARU_Window *)ctx->base.window_list_head);
  }

  if (ctx->window_class) {
    UnregisterClassW((LPCWSTR)(uintptr_t)ctx->window_class, ctx->instance);
  }

  if (ctx->wake_event) {
    CloseHandle(ctx->wake_event);
  }

  _maru_cleanup_context_base(&ctx->base);
  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_updateContext_Windows(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  _maru_update_context_base(&ctx->base, field_mask, attributes);
  return MARU_SUCCESS;
}

void *_maru_getContextNativeHandle_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  return (void *)ctx->instance;
}
