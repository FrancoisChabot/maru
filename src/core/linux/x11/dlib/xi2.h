// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_DLIB_XI2_H_INCLUDED
#define MARU_X11_DLIB_XI2_H_INCLUDED

#include "maru_internal.h"
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>

int XIQueryVersion(Display *display, int *major_version_inout,
                   int *minor_version_inout);
int XISelectEvents(Display *display, Window window, XIEventMask *masks,
                   int num_masks);

#define MARU_XI2_FUNCTIONS_TABLE \
  MARU_LIB_FN(XIQueryVersion)    \
  MARU_LIB_FN(XISelectEvents)

typedef struct MARU_Lib_Xi2 {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(name) *name;
  MARU_XI2_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Xi2;

#endif
