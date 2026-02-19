// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"
#include "maru_api_constraints.h"
#include "maru_mem_internal.h"

#include <stdlib.h>
#include <string.h>

static const char* _maru_win32_window_class = "MaruWindowClass";

static ATOM _maru_win32_register_class(HINSTANCE hinstance) {
  WNDCLASSA wc = { 0 };
  wc.lpfnWndProc = DefWindowProcA;
  wc.hInstance = hinstance;
  wc.lpszClassName = _maru_win32_window_class;
  return RegisterClassA(&wc);
}

static HWND _maru_win32_create_window(HINSTANCE hinstance,
                                       const MARU_WindowCreateInfo *create_info) {
  ATOM atom = _maru_win32_register_class(hinstance);
  if (!atom) {
    return NULL;
  }

  DWORD style = WS_OVERLAPPEDWINDOW;
  DWORD ex_style = 0;

  int width = 800;
  int height = 600;
  if (create_info->attributes.logical_size.x > 0 && create_info->attributes.logical_size.y > 0) {
    width = (int)create_info->attributes.logical_size.x;
    height = (int)create_info->attributes.logical_size.y;
  }

  const char* title = create_info->attributes.title ? create_info->attributes.title : "Maru Window";

  HWND hwnd = CreateWindowExA(
      ex_style,
      _maru_win32_window_class,
      title,
      style,
      CW_USEDEFAULT, CW_USEDEFAULT,
      width, height,
      NULL, NULL,
      hinstance,
      NULL);

  return hwnd;
}

MARU_Status maru_createWindow_Windows(MARU_Context *context,
                                      const MARU_WindowCreateInfo *create_info,
                                      MARU_Window **out_window) {
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)context;

  MARU_Window_Windows *window = (MARU_Window_Windows *)maru_context_alloc(
      &ctx->base, sizeof(MARU_Window_Windows));
  if (!window) {
    return MARU_FAILURE;
  }
  memset(((uint8_t*)window) + sizeof(MARU_Window_Base), 0, sizeof(MARU_Window_Windows) - sizeof(MARU_Window_Base));

  window->base.ctx_base = &ctx->base;
  window->base.pub.context = (MARU_Context *)ctx;
  window->base.pub.userdata = create_info->userdata;
#ifdef MARU_INDIRECT_BACKEND
  window->base.backend = ctx->base.backend;
#endif

  window->size = create_info->attributes.logical_size;
  if (window->size.x <= 0) window->size.x = 800;
  if (window->size.y <= 0) window->size.y = 600;

  HMODULE hinstance = GetModuleHandleA(NULL);
  window->hwnd = _maru_win32_create_window(hinstance, create_info);
  if (!window->hwnd) {
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

  window->hdc = GetDC(window->hwnd);
  if (!window->hdc) {
    DestroyWindow(window->hwnd);
    maru_context_free(&ctx->base, window);
    return MARU_FAILURE;
  }

  ShowWindow(window->hwnd, SW_SHOW);
  UpdateWindow(window->hwnd);

  window->base.pub.flags = MARU_WINDOW_STATE_READY;

  *out_window = (MARU_Window *)window;
  return MARU_SUCCESS;
}

MARU_Status maru_destroyWindow_Windows(MARU_Window *window_handle) {
  MARU_Window_Windows *window = (MARU_Window_Windows *)window_handle;
  MARU_Context_Windows *ctx = (MARU_Context_Windows *)window->base.ctx_base;

  if (window->hdc) {
    ReleaseDC(window->hwnd, window->hdc);
  }

  if (window->hwnd) {
    DestroyWindow(window->hwnd);
  }

  maru_context_free(&ctx->base, window);
  return MARU_SUCCESS;
}

MARU_Status maru_getWindowGeometry_Windows(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  const MARU_Window_Windows *window = (const MARU_Window_Windows *)window_handle;

  RECT rect = { 0, 0, 800, 600 };

  if (window->hwnd) {
    GetWindowRect(window->hwnd, &rect);
  }

  LONG width = rect.right - rect.left;
  LONG height = rect.bottom - rect.top;

  *out_geometry = (MARU_WindowGeometry){
      .origin = { (MARU_Scalar)rect.left, (MARU_Scalar)rect.top },
      .logical_size = { (MARU_Scalar)width, (MARU_Scalar)height },
      .pixel_size = { width, height },
  };

  return MARU_SUCCESS;
}
