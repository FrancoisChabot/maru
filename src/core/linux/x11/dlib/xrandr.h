// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_DLIB_XRANDR_H_INCLUDED
#define MARU_X11_DLIB_XRANDR_H_INCLUDED

#include "maru_internal.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>

#define MARU_XRANDR_FUNCTIONS_TABLE      \
  MARU_LIB_FN(XRRGetMonitors)            \
  MARU_LIB_FN(XRRFreeMonitors)           \
  MARU_LIB_FN(XRRGetScreenResourcesCurrent) \
  MARU_LIB_FN(XRRFreeScreenResources)    \
  MARU_LIB_FN(XRRGetOutputInfo)          \
  MARU_LIB_FN(XRRFreeOutputInfo)         \
  MARU_LIB_FN(XRRGetCrtcInfo)            \
  MARU_LIB_FN(XRRFreeCrtcInfo)           \
  MARU_LIB_FN(XRRGetOutputPrimary)       \
  MARU_LIB_FN(XRRSetCrtcConfig)

typedef struct MARU_Lib_Xrandr {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(name) *name;
  MARU_XRANDR_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_Xrandr;

#endif
