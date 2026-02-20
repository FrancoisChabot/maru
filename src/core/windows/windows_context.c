// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

static HMODULE _maru_win32_get_user32(void) {
  return GetModuleHandleA("user32.dll");
}

MARU_Status maru_createContext_Windows(const MARU_ContextCreateInfo *create_info,
                                        MARU_Context **out_context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)maru_context_alloc_bootstrap(
      create_info, sizeof(MARU_Context_Windows));
  if (!ctx) {
    return MARU_FAILURE;
  }

  memset(((uint8_t*)ctx) + sizeof(MARU_Context_Base), 0, sizeof(MARU_Context_Windows) - sizeof(MARU_Context_Base));

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
  ctx->base.event_cb = create_info->attributes.event_callback;
  ctx->base.event_mask = create_info->attributes.event_mask;

  ctx->base.metrics.user_events = &ctx->base.user_event_metrics;
  ctx->base.pub.metrics = &ctx->base.metrics;

  if (create_info->tuning) {
    ctx->base.tuning = *create_info->tuning;
  } else {
    MARU_ContextTuning default_tuning = MARU_CONTEXT_TUNING_DEFAULT;
    ctx->base.tuning = default_tuning;
  }

#ifdef MARU_INDIRECT_BACKEND
  extern const MARU_Backend maru_backend_Windows;
  ctx->base.backend = &maru_backend_Windows;
#endif

#ifdef MARU_VALIDATE_API_CALLS
  extern MARU_ThreadId _maru_getCurrentThreadId(void);
  ctx->base.creator_thread = _maru_getCurrentThreadId();
#endif

  ctx->user32_module = _maru_win32_get_user32();
  if (!ctx->user32_module) {
    _maru_reportDiagnostic((MARU_Context*)ctx, MARU_DIAGNOSTIC_RESOURCE_UNAVAILABLE, "Failed to load user32.dll");
    maru_context_free(&ctx->base, ctx);
    return MARU_FAILURE;
  }

  ctx->owner_thread_id = GetCurrentThreadId();

  ctx->base.pub.flags = MARU_CONTEXT_STATE_READY;
  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyContext_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  if (ctx->user32_module) {
  }

  maru_context_free(&ctx->base, context);
  return MARU_SUCCESS;
}

MARU_Status maru_pumpEvents_Windows(MARU_Context *context, uint32_t timeout_ms) {
  (void)context;
  
  MSG msg;
  if (timeout_ms == 0) {
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
  } else {
    DWORD start_time = GetTickCount();
    DWORD elapsed = 0;

    while (elapsed < timeout_ms) {
      if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
      } else {
        MsgWaitForMultipleObjects(0, NULL, FALSE, timeout_ms - elapsed, QS_ALLINPUT);
      }
      elapsed = GetTickCount() - start_time;
    }
  }
  
  return MARU_SUCCESS;
}

MARU_Status maru_updateContext_Windows(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  _maru_update_context_base(&ctx->base, field_mask, attributes);

  if (field_mask & MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE) {
    // TODO: Implement idle inhibition
  }

  return MARU_SUCCESS;
}

MARU_Status maru_getStandardCursor_Windows(MARU_Context *context, MARU_CursorShape shape,
                                            MARU_Cursor **out_cursor) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  LPCSTR id;

  switch (shape) {
    case MARU_CURSOR_SHAPE_DEFAULT: id = IDC_ARROW; break;
    case MARU_CURSOR_SHAPE_HELP: id = IDC_HELP; break;
    case MARU_CURSOR_SHAPE_HAND: id = IDC_HAND; break;
    case MARU_CURSOR_SHAPE_WAIT: id = IDC_WAIT; break;
    case MARU_CURSOR_SHAPE_CROSSHAIR: id = IDC_CROSS; break;
    case MARU_CURSOR_SHAPE_TEXT: id = IDC_IBEAM; break;
    case MARU_CURSOR_SHAPE_MOVE: id = IDC_SIZEALL; break;
    case MARU_CURSOR_SHAPE_NOT_ALLOWED: id = IDC_NO; break;
    case MARU_CURSOR_SHAPE_EW_RESIZE: id = IDC_SIZEWE; break;
    case MARU_CURSOR_SHAPE_NS_RESIZE: id = IDC_SIZENS; break;
    case MARU_CURSOR_SHAPE_NESW_RESIZE: id = IDC_SIZENESW; break;
    case MARU_CURSOR_SHAPE_NWSE_RESIZE: id = IDC_SIZENWSE; break;
    default: return MARU_FAILURE;
  }

  HCURSOR hcursor = LoadCursor(NULL, id);
  if (!hcursor) {
    return MARU_FAILURE;
  }

  MARU_Cursor_Windows *cursor = (MARU_Cursor_Windows *)maru_context_alloc(&ctx->base, sizeof(MARU_Cursor_Windows));
  if (!cursor) {
    return MARU_FAILURE;
  }
  memset(cursor, 0, sizeof(MARU_Cursor_Windows));

  cursor->base.ctx_base = &ctx->base;
  cursor->base.pub.metrics = &cursor->base.metrics;
  cursor->base.pub.flags = MARU_CURSOR_FLAG_SYSTEM;
#ifdef MARU_INDIRECT_BACKEND
  cursor->base.backend = ctx->base.backend;
#endif
  cursor->hcursor = hcursor;

  *out_cursor = (MARU_Cursor *)cursor;
  return MARU_SUCCESS;
}

MARU_Status maru_wakeContext_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  PostThreadMessageA(ctx->owner_thread_id, WM_NULL, 0, 0);
  return MARU_SUCCESS;
}
