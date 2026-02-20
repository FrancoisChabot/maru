// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LIBDECOR_DLIB_H_INCLUDED
#define MARU_LIBDECOR_DLIB_H_INCLUDED

#include "wayland-client.h"
#include "vendor/libdecor.h"
#include "maru_internal.h"

struct libdecor;
void libdecor_set_userdata(struct libdecor *context, void *userdata);
void *libdecor_get_userdata(struct libdecor *context);

#define MARU_LIBDECOR_FUNCTIONS_TABLE         \
  MARU_LIB_FN(new)                            \
  MARU_LIB_FN(unref)                          \
  MARU_LIB_FN(decorate)                       \
  MARU_LIB_FN(frame_unref)                    \
  MARU_LIB_FN(frame_set_title)               \
  MARU_LIB_FN(frame_set_app_id)              \
  MARU_LIB_FN(frame_set_capabilities)         \
  MARU_LIB_FN(frame_map)                     \
  MARU_LIB_FN(frame_set_visibility)          \
  MARU_LIB_FN(frame_commit)                  \
  MARU_LIB_FN(frame_get_xdg_toplevel)        \
  MARU_LIB_FN(frame_get_xdg_surface)         \
  MARU_LIB_FN(frame_set_min_content_size)    \
  MARU_LIB_FN(frame_set_max_content_size)    \
  MARU_LIB_FN(frame_get_min_content_size)    \
  MARU_LIB_FN(frame_get_max_content_size)    \
  MARU_LIB_FN(frame_set_fullscreen)          \
  MARU_LIB_FN(frame_unset_fullscreen)        \
  MARU_LIB_FN(frame_set_maximized)           \
  MARU_LIB_FN(frame_unset_maximized)         \
  MARU_LIB_FN(frame_translate_coordinate)    \
  MARU_LIB_FN(state_new)                      \
  MARU_LIB_FN(state_free)                     \
  MARU_LIB_FN(configuration_get_content_size) \
  MARU_LIB_FN(configuration_get_window_state) \
  MARU_LIB_FN(dispatch)                       \
  MARU_LIB_FN(get_fd)                         \
  MARU_LIB_OPT_FN(set_userdata)               \
  MARU_LIB_OPT_FN(get_userdata)

typedef struct MARU_Lib_Decor {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(libdecor_##name) *name;
#define MARU_LIB_OPT_FN(name)
  MARU_LIBDECOR_FUNCTIONS_TABLE
#undef MARU_LIB_FN
#undef MARU_LIB_OPT_FN

  struct {
#define MARU_LIB_FN(name)
#define MARU_LIB_OPT_FN(name) __typeof__(libdecor_##name) *name;
    MARU_LIBDECOR_FUNCTIONS_TABLE
#undef MARU_LIB_FN
#undef MARU_LIB_OPT_FN
  } opt;
} MARU_Lib_Decor;

#endif
