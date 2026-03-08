// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LINUX_NATIVE_STUBS_H_INCLUDED
#define MARU_LINUX_NATIVE_STUBS_H_INCLUDED

#include "maru_internal.h"
#include "maru/c/native/wayland.h"
#include "maru/c/native/x11.h"

static inline MARU_Status _maru_stub_x11_context_handle_unsupported(
    MARU_X11ContextHandle *out_handle) {
  out_handle->display = NULL;
  return MARU_FAILURE;
}

static inline MARU_Status _maru_stub_x11_window_handle_unsupported(
    MARU_X11WindowHandle *out_handle) {
  out_handle->display = NULL;
  out_handle->window = 0;
  return MARU_FAILURE;
}

static inline MARU_Status _maru_stub_wayland_context_handle_unsupported(
    MARU_WaylandContextHandle *out_handle) {
  out_handle->display = NULL;
  return MARU_FAILURE;
}

static inline MARU_Status _maru_stub_wayland_window_handle_unsupported(
    MARU_WaylandWindowHandle *out_handle) {
  out_handle->display = NULL;
  out_handle->surface = NULL;
  return MARU_FAILURE;
}

#endif
