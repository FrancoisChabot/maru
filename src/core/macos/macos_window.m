// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"

MARU_Status maru_createWindow_Cocoa(MARU_Context *context,
                                      const MARU_WindowCreateInfo *create_info,
                                      MARU_Window **out_window) {
  return MARU_FAILURE;
}

MARU_Status maru_destroyWindow_Cocoa(MARU_Window *window) {
  return MARU_FAILURE;
}

MARU_Status maru_getWindowGeometry_Cocoa(MARU_Window *window, MARU_WindowGeometry *out_geometry) {
  return MARU_FAILURE;
}

MARU_Status maru_updateWindow_Cocoa(MARU_Window *window, uint64_t field_mask,
                                      const MARU_WindowAttributes *attributes) {
  return MARU_FAILURE;
}

MARU_Status maru_requestWindowFocus_Cocoa(MARU_Window *window) {
  return MARU_FAILURE;
}

MARU_Status maru_requestWindowFrame_Cocoa(MARU_Window *window) {
  return MARU_FAILURE;
}

MARU_Status maru_requestWindowAttention_Cocoa(MARU_Window *window) {
  return MARU_FAILURE;
}

void *_maru_getWindowNativeHandle_Cocoa(MARU_Window *window) {
  return NULL;
}
