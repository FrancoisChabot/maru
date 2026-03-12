// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LINUX_NATIVE_STUBS_H_INCLUDED
#define MARU_LINUX_NATIVE_STUBS_H_INCLUDED

#include "maru_internal.h"
#include "maru/native/wayland.h"
#include "maru/native/x11.h"

static inline MARU_X11ContextHandle _maru_stub_x11_context_handle_unsupported(
    void) {
  MARU_X11ContextHandle handle = {0};
  return handle;
}

static inline MARU_X11WindowHandle _maru_stub_x11_window_handle_unsupported(
    void) {
  MARU_X11WindowHandle handle = {0};
  return handle;
}

static inline bool _maru_stub_x11_extended_frame_sync_unsupported(void) {
  return false;
}

static inline MARU_WaylandContextHandle
_maru_stub_wayland_context_handle_unsupported(void) {
  MARU_WaylandContextHandle handle = {0};
  return handle;
}

static inline MARU_WaylandWindowHandle
_maru_stub_wayland_window_handle_unsupported(void) {
  MARU_WaylandWindowHandle handle = {0};
  return handle;
}

#endif
