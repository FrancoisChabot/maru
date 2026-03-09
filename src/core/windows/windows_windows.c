// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "windows_internal.h"

MARU_Status maru_createWindow_Windows(MARU_Context *context,
                                       const MARU_WindowCreateInfo *create_info,
                                       MARU_Window **out_window) {
  (void)context;
  (void)create_info;
  (void)out_window;
  return MARU_FAILURE;
}

MARU_Status maru_destroyWindow_Windows(MARU_Window *window) {
  (void)window;
  return MARU_FAILURE;
}

void maru_getWindowGeometry_Windows(MARU_Window *window_handle, MARU_WindowGeometry *out_geometry) {
  (void)window_handle;
  if (out_geometry) {
    memset(out_geometry, 0, sizeof(MARU_WindowGeometry));
  }
}

MARU_Status maru_updateWindow_Windows(MARU_Window *window, uint64_t field_mask,
                                       const MARU_WindowAttributes *attributes) {
  (void)window;
  (void)field_mask;
  (void)attributes;
  return MARU_FAILURE;
}

MARU_Status maru_requestWindowFocus_Windows(MARU_Window *window) {
  (void)window;
  return MARU_FAILURE;
}

MARU_Status maru_requestWindowFrame_Windows(MARU_Window *window) {
  (void)window;
  return MARU_FAILURE;
}

MARU_Status maru_requestWindowAttention_Windows(MARU_Window *window) {
  (void)window;
  return MARU_FAILURE;
}

void *_maru_getWindowNativeHandle_Windows(MARU_Window *window) {
  (void)window;
  return NULL;
}
