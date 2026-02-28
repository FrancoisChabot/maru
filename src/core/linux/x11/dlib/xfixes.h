// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_DLIB_XFIXES_H_INCLUDED
#define MARU_X11_DLIB_XFIXES_H_INCLUDED

#include "maru_internal.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#define MARU_XFIXES_FUNCTIONS_TABLE   \
  MARU_LIB_FN(XFixesQueryExtension)   \
  MARU_LIB_FN(XFixesQueryVersion)     \
  MARU_LIB_FN(XFixesSetWindowShapeRegion)

typedef struct MARU_Lib_Xfixes {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(name) *name;
  MARU_XFIXES_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Xfixes;

#endif
