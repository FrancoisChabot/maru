// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WAYLAND_CURSOR_DLIB_H_INCLUDED
#define MARU_WAYLAND_CURSOR_DLIB_H_INCLUDED

#include "maru_internal.h"
#include "vendor/wayland-cursor.h"

#define MARU_WL_CURSOR_FUNCTIONS_TABLE \
  MARU_LIB_FN(theme_load)              \
  MARU_LIB_FN(theme_destroy)           \
  MARU_LIB_FN(theme_get_cursor)        \
  MARU_LIB_FN(image_get_buffer)

typedef struct MARU_Lib_WaylandCursor {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(wl_cursor_##name) *name;
  MARU_WL_CURSOR_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_WaylandCursor;

#endif
