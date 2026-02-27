// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_DLIB_H_INCLUDED
#define MARU_X11_DLIB_H_INCLUDED

#include "maru_internal.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

#define MARU_X11_FUNCTIONS_TABLE           \
  MARU_LIB_FN(XOpenDisplay)                \
  MARU_LIB_FN(XCloseDisplay)               \
  MARU_LIB_FN(XPending)                    \
  MARU_LIB_FN(XNextEvent)                  \
  MARU_LIB_FN(XConnectionNumber)           \
  MARU_LIB_FN(XFlush)                      \
  MARU_LIB_FN(XCreateSimpleWindow)         \
  MARU_LIB_FN(XCreateWindow)               \
  MARU_LIB_FN(XCreateColormap)             \
  MARU_LIB_FN(XFreeColormap)               \
  MARU_LIB_FN(XDestroyWindow)              \
  MARU_LIB_FN(XMapWindow)                  \
  MARU_LIB_FN(XUnmapWindow)                \
  MARU_LIB_FN(XDefineCursor)               \
  MARU_LIB_FN(XUndefineCursor)             \
  MARU_LIB_FN(XCreateFontCursor)           \
  MARU_LIB_FN(XFreeCursor)                 \
  MARU_LIB_FN(XSelectInput)                \
  MARU_LIB_FN(XInternAtom)                 \
  MARU_LIB_FN(XSendEvent)                  \
  MARU_LIB_FN(XSetWMProtocols)             \
  MARU_LIB_FN(XSetWMNormalHints)           \
  MARU_LIB_FN(XStoreName)                  \
  MARU_LIB_FN(XResizeWindow)               \
  MARU_LIB_FN(XMoveWindow)                 \
  MARU_LIB_FN(XIconifyWindow)              \
  MARU_LIB_FN(XTranslateCoordinates)       \
  MARU_LIB_FN(XLookupString)               \
  MARU_LIB_FN(XSync)                       \
  MARU_LIB_FN(XRaiseWindow)                \
  MARU_LIB_FN(XSetInputFocus)              \
  MARU_LIB_FN(XFilterEvent)

typedef struct MARU_Lib_X11 {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(name) *name;
  MARU_X11_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_X11;

#endif
