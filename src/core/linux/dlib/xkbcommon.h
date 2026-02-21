// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_XKBCOMMON_DLIB_H_INCLUDED
#define MARU_XKBCOMMON_DLIB_H_INCLUDED

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

#include "maru_internal.h"

#define MARU_XKB_FUNCTIONS_TABLE        \
  MARU_LIB_FN(context_new)              \
  MARU_LIB_FN(context_unref)            \
  MARU_LIB_FN(keymap_new_from_string)   \
  MARU_LIB_FN(keymap_unref)             \
  MARU_LIB_FN(state_new)                \
  MARU_LIB_FN(state_unref)              \
  MARU_LIB_FN(state_update_mask)        \
  MARU_LIB_FN(state_key_get_one_sym)    \
  MARU_LIB_FN(state_key_get_utf8)       \
  MARU_LIB_FN(state_mod_name_is_active) \
  MARU_LIB_FN(state_mod_index_is_active) \
  MARU_LIB_FN(keymap_mod_get_index)     \
  MARU_LIB_FN(state_serialize_mods)

typedef struct MARU_Lib_Xkb {
  MARU_External_Lib_Base base;

#define MARU_LIB_FN(name) __typeof__(xkb_##name) *name;
  MARU_XKB_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Xkb;

#endif
