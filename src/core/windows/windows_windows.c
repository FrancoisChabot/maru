// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_mem_internal.h"
#include <string.h>

LRESULT CALLBACK _maru_window_proc(HWND hwnd, UINT uMsg, WPARAM wParam,
                                  LPARAM lParam) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
  if (!win) {
    if (uMsg == WM_CREATE) {
        LPCREATESTRUCTW cs = (LPCREATESTRUCTW)lParam;
        win = (MARU_Window_Windows *)cs->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)win);
        win->hwnd = hwnd;
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
  }

  MARU_Context_Windows *ctx = (MARU_Context_Windows *)win->base.ctx_base;

  switch (uMsg) {
    case WM_CLOSE: {
      MARU_Event evt = {0};
      _maru_dispatch_event(&ctx->base, MARU_EVENT_CLOSE_REQUESTED, (MARU_Window *)win, &evt);
      return 0;
    }

    case WM_DESTROY: {
      SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
      win->hwnd = NULL;
      return 0;
    }
    
    case WM_PAINT: {
        ValidateRect(hwnd, NULL);
        return 0;
    }

    case WM_ERASEBKGND:
      return 1;
  }

  return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

MARU_Status maru_createWindow_Windows(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  MARU_Window_Windows *win = (MARU_Window_Windows *)maru_context_alloc(
      &ctx->base, sizeof(MARU_Window_Windows));
  if (!win) {
    return MARU_FAILURE;
  }
  memset(win, 0, sizeof(MARU_Window_Windows));

  win->base.ctx_base = &ctx->base;
  win->base.attrs_requested = create_info->attributes;
  win->base.attrs_effective = create_info->attributes;
  win->base.pending_ready_event = true;

  // Initialize public-facing (exposed) part of the window
  win->base.pub.context = context;
  win->base.pub.userdata = create_info->userdata;
  win->base.pub.metrics = &win->base.metrics;
  win->base.pub.keyboard_state = win->base.keyboard_state;
  win->base.pub.keyboard_key_count = MARU_KEY_COUNT;
  win->base.pub.mouse_button_state = ctx->base.pub.mouse_button_state;
  win->base.pub.mouse_button_channels = ctx->base.pub.mouse_button_channels;
  win->base.pub.mouse_button_count = ctx->base.pub.mouse_button_count;
  memcpy(win->base.pub.mouse_default_button_channels,
         ctx->base.pub.mouse_default_button_channels,
         sizeof(win->base.pub.mouse_default_button_channels));
  win->base.pub.event_mask = create_info->attributes.event_mask;
  win->base.pub.cursor_mode = create_info->attributes.cursor_mode;
  win->base.pub.current_cursor = (MARU_Cursor *)create_info->attributes.cursor;
  win->base.pub.title = NULL; // Updated when title is changed or requested

  wchar_t title_w[256];
  if (create_info->attributes.title && create_info->attributes.title[0] != '\0') {
    MultiByteToWideChar(CP_UTF8, 0, create_info->attributes.title, -1, title_w, 256);
  } else {
    wcscpy(title_w, L"Maru Window");
  }

  DWORD style = WS_OVERLAPPEDWINDOW;
  DWORD ex_style = WS_EX_APPWINDOW;

  int x = CW_USEDEFAULT;
  int y = CW_USEDEFAULT;
  int w = (int)create_info->attributes.logical_size.x;
  int h = (int)create_info->attributes.logical_size.y;

  if (w == 0 || h == 0) {
      w = 800;
      h = 600;
  }

  RECT rect = {0, 0, w, h};
  AdjustWindowRectEx(&rect, style, FALSE, ex_style);
  w = rect.right - rect.left;
  h = rect.bottom - rect.top;

  win->hwnd = CreateWindowExW(
      ex_style, L"MARU_WindowClass", title_w, style,
      x, y, w, h,
      NULL, NULL, ctx->instance, win);

  if (!win->hwnd) {
    maru_context_free(&ctx->base, win);
    return MARU_FAILURE;
  }

  win->hdc = GetDC(win->hwnd);
  
  _maru_register_window(&ctx->base, (MARU_Window *)win);

  // If visible was requested, we'll show it, but we delay it
  // until the first event pump to avoid the white flash during initialization.
  if (create_info->attributes.visible) {
    win->show_on_first_pump = true;
    win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
  }

  *out_window = (MARU_Window *)win;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_Windows(MARU_Window *window) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)win->base.ctx_base;

  if (win->hwnd) {
    ReleaseDC(win->hwnd, win->hdc);
    DestroyWindow(win->hwnd);
  }

  _maru_unregister_window(&ctx->base, window);
  maru_context_free(&ctx->base, window);
  return MARU_SUCCESS;
}

void maru_getWindowGeometry_Windows(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window_handle;
  if (!win->hwnd || !out_geometry) return;

  RECT rect;
  GetClientRect(win->hwnd, &rect);
  int w = rect.right - rect.left;
  int h = rect.bottom - rect.top;

  out_geometry->pixel_size.x = (int32_t)w;
  out_geometry->pixel_size.y = (int32_t)h;
  
  // For now, scale is 1.0. 
  // TODO: Add DPI support
  out_geometry->scale = (MARU_Scalar)1.0;
  
  out_geometry->logical_size.x = (MARU_Scalar)w;
  out_geometry->logical_size.y = (MARU_Scalar)h;

  POINT pt = {0, 0};
  ClientToScreen(win->hwnd, &pt);
  out_geometry->origin.x = (MARU_Scalar)pt.x;
  out_geometry->origin.y = (MARU_Scalar)pt.y;
  
  out_geometry->buffer_transform = MARU_BUFFER_TRANSFORM_NORMAL;
}

MARU_Status maru_updateWindow_Windows(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  
  if (field_mask & MARU_WINDOW_ATTR_TITLE) {
    if (attributes->title) {
      wchar_t title_w[256];
      MultiByteToWideChar(CP_UTF8, 0, attributes->title, -1, title_w, 256);
      SetWindowTextW(win->hwnd, title_w);
      win->base.pub.title = attributes->title; // Note: this is a shallow copy, user must keep it alive or we need storage
    }
  }

  if (field_mask & MARU_WINDOW_ATTR_VISIBLE) {
    if (attributes->visible) {
      ShowWindow(win->hwnd, SW_SHOW);
    } else {
      ShowWindow(win->hwnd, SW_HIDE);
    }
    if (attributes->visible) {
        win->base.pub.flags |= MARU_WINDOW_STATE_VISIBLE;
    } else {
        win->base.pub.flags &= ~MARU_WINDOW_STATE_VISIBLE;
    }
  }

  // TODO: Implement other attributes (resize, fullscreen, etc)
  win->base.attrs_effective = *attributes;

  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFocus_Windows(MARU_Window *window) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  if (win->hwnd) {
    SetForegroundWindow(win->hwnd);
    SetFocus(win->hwnd);
  }
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowFrame_Windows(MARU_Window *window) {
  (void)window;
  return MARU_SUCCESS;
}

MARU_Status maru_requestWindowAttention_Windows(MARU_Window *window) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  if (win->hwnd) {
    FlashWindow(win->hwnd, TRUE);
  }
  return MARU_SUCCESS;
}

void *_maru_getWindowNativeHandle_Windows(MARU_Window *window) {
  MARU_Window_Windows *win = (MARU_Window_Windows *)window;
  return (void *)win->hwnd;
}
