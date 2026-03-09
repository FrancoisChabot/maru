// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <string.h>

static void _maru_windows_init_mouse_button_channels(MARU_Context_Windows *ctx) {
  const uint32_t count = 5;
  ctx->base.mouse_button_channels = (MARU_MouseButtonChannelInfo *)maru_context_alloc(
      &ctx->base, sizeof(MARU_MouseButtonChannelInfo) * count);
  if (!ctx->base.mouse_button_channels) {
    return;
  }
  memset(ctx->base.mouse_button_channels, 0, sizeof(MARU_MouseButtonChannelInfo) * count);

  ctx->base.mouse_button_states = (MARU_ButtonState8 *)maru_context_alloc(
      &ctx->base, sizeof(MARU_ButtonState8) * count);
  if (!ctx->base.mouse_button_states) {
    maru_context_free(&ctx->base, ctx->base.mouse_button_channels);
    ctx->base.mouse_button_channels = NULL;
    return;
  }
  memset(ctx->base.mouse_button_states, 0, sizeof(MARU_ButtonState8) * count);

  ctx->base.mouse_button_channels[0].name = "Left";
  ctx->base.mouse_button_channels[0].native_code = VK_LBUTTON;
  ctx->base.mouse_button_channels[0].is_default = true;

  ctx->base.mouse_button_channels[1].name = "Right";
  ctx->base.mouse_button_channels[1].native_code = VK_RBUTTON;
  ctx->base.mouse_button_channels[1].is_default = true;

  ctx->base.mouse_button_channels[2].name = "Middle";
  ctx->base.mouse_button_channels[2].native_code = VK_MBUTTON;
  ctx->base.mouse_button_channels[2].is_default = true;

  ctx->base.mouse_button_channels[3].name = "X1";
  ctx->base.mouse_button_channels[3].native_code = VK_XBUTTON1;
  ctx->base.mouse_button_channels[3].is_default = true;

  ctx->base.mouse_button_channels[4].name = "X2";
  ctx->base.mouse_button_channels[4].native_code = VK_XBUTTON2;
  ctx->base.mouse_button_channels[4].is_default = true;

  ctx->base.pub.mouse_button_channels = ctx->base.mouse_button_channels;
  ctx->base.pub.mouse_button_state = ctx->base.mouse_button_states;
  ctx->base.pub.mouse_button_count = count;

  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_LEFT] = 0;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_RIGHT] = 1;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_MIDDLE] = 2;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_BACK] = 3;
  ctx->base.pub.mouse_default_button_channels[MARU_MOUSE_DEFAULT_FORWARD] = 4;
}

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

  _maru_windows_init_mouse_button_channels(ctx);

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
