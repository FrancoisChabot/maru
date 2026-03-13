// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <string.h>

static void _maru_windows_init_mouse_button_channels(MARU_Context_Windows *ctx) {
  const uint32_t count = 5;
  ctx->base.mouse_button_channels = (MARU_ChannelInfo *)maru_context_alloc(
      &ctx->base, sizeof(MARU_ChannelInfo) * count);
  if (!ctx->base.mouse_button_channels) {
    return;
  }
  memset(ctx->base.mouse_button_channels, 0, sizeof(MARU_ChannelInfo) * count);

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
  ctx->base.mouse_button_channels[0].flags = MARU_CHANNEL_FLAG_IS_DEFAULT;
  ctx->base.mouse_button_channels[0].min_value = 0.0f;
  ctx->base.mouse_button_channels[0].max_value = 1.0f;

  ctx->base.mouse_button_channels[1].name = "Right";
  ctx->base.mouse_button_channels[1].native_code = VK_RBUTTON;
  ctx->base.mouse_button_channels[1].flags = MARU_CHANNEL_FLAG_IS_DEFAULT;
  ctx->base.mouse_button_channels[1].min_value = 0.0f;
  ctx->base.mouse_button_channels[1].max_value = 1.0f;

  ctx->base.mouse_button_channels[2].name = "Middle";
  ctx->base.mouse_button_channels[2].native_code = VK_MBUTTON;
  ctx->base.mouse_button_channels[2].flags = MARU_CHANNEL_FLAG_IS_DEFAULT;
  ctx->base.mouse_button_channels[2].min_value = 0.0f;
  ctx->base.mouse_button_channels[2].max_value = 1.0f;

  ctx->base.mouse_button_channels[3].name = "X1";
  ctx->base.mouse_button_channels[3].native_code = VK_XBUTTON1;
  ctx->base.mouse_button_channels[3].flags = MARU_CHANNEL_FLAG_IS_DEFAULT;
  ctx->base.mouse_button_channels[3].min_value = 0.0f;
  ctx->base.mouse_button_channels[3].max_value = 1.0f;

  ctx->base.mouse_button_channels[4].name = "X2";
  ctx->base.mouse_button_channels[4].native_code = VK_XBUTTON2;
  ctx->base.mouse_button_channels[4].flags = MARU_CHANNEL_FLAG_IS_DEFAULT;
  ctx->base.mouse_button_channels[4].min_value = 0.0f;
  ctx->base.mouse_button_channels[4].max_value = 1.0f;

  ctx->base.pub.mouse_button_channels = ctx->base.mouse_button_channels;
  ctx->base.pub.mouse_button_state = ctx->base.mouse_button_states;
  ctx->base.pub.mouse_button_count = count;
}

static void _maru_windows_apply_idle_inhibit(MARU_Context_Windows *ctx) {
  if (ctx->base.inhibit_idle) {
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
  } else {
    SetThreadExecutionState(ES_CONTINUOUS);
  }
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
  ctx->base.pub.userdata = create_info->userdata;

  _maru_windows_init_mouse_button_channels(ctx);

  ctx->user32_module = GetModuleHandleW(L"user32.dll");
  if (ctx->user32_module) {
    ctx->SetProcessDpiAwarenessContext = (BOOL(WINAPI *)(HANDLE))GetProcAddress(ctx->user32_module, "SetProcessDpiAwarenessContext");
    ctx->GetDpiForWindow = (UINT(WINAPI *)(HWND))GetProcAddress(ctx->user32_module, "GetDpiForWindow");
    ctx->EnableNonClientDpiScaling = (BOOL(WINAPI *)(HWND))GetProcAddress(ctx->user32_module, "EnableNonClientDpiScaling");
  }

  ctx->shcore_module = LoadLibraryW(L"shcore.dll");
  if (ctx->shcore_module) {
    ctx->GetDpiForMonitor = (HRESULT(WINAPI *)(HMONITOR, int, UINT*, UINT*))GetProcAddress(ctx->shcore_module, "GetDpiForMonitor");
  }

  if (ctx->SetProcessDpiAwarenessContext) {
    ctx->SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  }

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

  ctx->base.pub.flags = 0;
  ctx->base.attrs_requested = create_info->attributes;
  ctx->base.attrs_effective = create_info->attributes;
  ctx->base.attrs_dirty_mask = 0;
  ctx->base.diagnostic_cb = create_info->attributes.diagnostic_cb;
  ctx->base.diagnostic_userdata = create_info->attributes.diagnostic_userdata;
  ctx->base.inhibit_idle = create_info->attributes.inhibit_idle;

  _maru_windows_apply_idle_inhibit(ctx);

  maru_updateMonitors_Windows((MARU_Context *)ctx);

  *out_context = (MARU_Context *)ctx;
  return MARU_SUCCESS;
}

void maru_destroyContext_Windows(MARU_Context *context) {
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

  if (ctx->shcore_module) {
    FreeLibrary(ctx->shcore_module);
  }

  _maru_windows_cleanup_controllers(ctx);

  _maru_windows_drain_deferred_events(ctx);

  if (ctx->clipboard_mime_query_storage) {
    maru_context_free(&ctx->base, ctx->clipboard_mime_query_storage);
  }
  if (ctx->clipboard_mime_query_ptr) {
    maru_context_free(&ctx->base, (void *)ctx->clipboard_mime_query_ptr);
  }

  // Reset idle inhibition
  ctx->base.inhibit_idle = false;
  _maru_windows_apply_idle_inhibit(ctx);

  _maru_cleanup_context_base(&ctx->base);
  maru_context_free(&ctx->base, context);
}

MARU_Status maru_updateContext_Windows(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  _maru_update_context_base(&ctx->base, field_mask, attributes);

  if (field_mask & MARU_CONTEXT_ATTR_INHIBIT_IDLE) {
    _maru_windows_apply_idle_inhibit(ctx);
  }

  return MARU_SUCCESS;
}

void *_maru_getContextNativeHandle_Windows(MARU_Context *context) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;
  return (void *)ctx->instance;
}
