// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_DLIB_XSHAPE_H_INCLUDED
#define MARU_X11_DLIB_XSHAPE_H_INCLUDED

#include "maru_internal.h"
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#define MARU_XSHAPE_FUNCTIONS_TABLE   \
  MARU_LIB_FN(XShapeQueryExtension)   \
  MARU_LIB_FN(XShapeCombineRectangles)

typedef struct MARU_Lib_Xshape {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(name) *name;
  MARU_XSHAPE_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Xshape;

#endif
