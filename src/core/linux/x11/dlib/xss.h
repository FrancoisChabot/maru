// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_DLIB_XSS_H_INCLUDED
#define MARU_X11_DLIB_XSS_H_INCLUDED

#include "maru_internal.h"
#include <X11/Xlib.h>

extern Bool XScreenSaverQueryExtension(Display *display, int *event_basep,
                                       int *error_basep);
extern void XScreenSaverSuspend(Display *display, Bool suspend);

#define MARU_XSS_FUNCTIONS_TABLE          \
  MARU_LIB_FN(XScreenSaverQueryExtension) \
  MARU_LIB_FN(XScreenSaverSuspend)

typedef struct MARU_Lib_Xss {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(name) *name;
  MARU_XSS_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Xss;

#endif
