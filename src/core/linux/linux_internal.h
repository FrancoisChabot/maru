// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LINUX_INTERNAL_H_INCLUDED
#define MARU_LINUX_INTERNAL_H_INCLUDED

#include <poll.h>
#include "dlib/xkbcommon.h"
#include "maru_internal.h"

typedef struct MARU_Context_Linux_Common {
  struct {
    struct xkb_context *ctx;
    struct xkb_keymap *keymap;
    struct xkb_state *state;
  } xkb;

  MARU_Lib_Xkb xkb_lib;
} MARU_Context_Linux_Common;

#endif
