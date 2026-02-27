// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_DLIB_XCURSOR_H_INCLUDED
#define MARU_X11_DLIB_XCURSOR_H_INCLUDED

#include "maru_internal.h"
#include <X11/Xlib.h>
#include <stdint.h>

typedef uint32_t XcursorUInt;
typedef XcursorUInt XcursorDim;
typedef XcursorUInt XcursorPixel;

typedef struct _XcursorImage {
  XcursorUInt version;
  XcursorDim size;
  XcursorDim width;
  XcursorDim height;
  XcursorDim xhot;
  XcursorDim yhot;
  XcursorUInt delay;
  XcursorPixel *pixels;
} XcursorImage;

XcursorImage *XcursorImageCreate(int width, int height);
Cursor XcursorImageLoadCursor(Display *display, const XcursorImage *image);
void XcursorImageDestroy(XcursorImage *image);

#define MARU_XCURSOR_FUNCTIONS_TABLE                    \
  MARU_LIB_FN(XcursorImageCreate)                       \
  MARU_LIB_FN(XcursorImageLoadCursor)                   \
  MARU_LIB_FN(XcursorImageDestroy)

typedef struct MARU_Lib_Xcursor {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(name) *name;
  MARU_XCURSOR_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Xcursor;

#endif
