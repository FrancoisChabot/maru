// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_INTERNAL_H_INCLUDED
#define MARU_X11_INTERNAL_H_INCLUDED

#include "linux_internal.h"

typedef struct MARU_Context_X11 {
  MARU_Context_Base base;
  MARU_Context_Linux_Common linux_common;

  MARU_Controller **controller_list_storage;
  uint32_t controller_list_capacity;
} MARU_Context_X11;

#endif
