// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_DLIB_LOADER_H_INCLUDED
#define MARU_X11_DLIB_LOADER_H_INCLUDED

#include <stdbool.h>
#include "x11.h"
#include "xcursor.h"
#include "xi2.h"
#include "xshape.h"
#include "xrandr.h"
#include "xfixes.h"

struct MARU_Context_Base;

bool maru_load_x11_symbols(struct MARU_Context_Base *ctx, MARU_Lib_X11 *x11);
void maru_unload_x11_symbols(MARU_Lib_X11 *x11);
bool maru_load_xcursor_symbols(struct MARU_Context_Base *ctx, MARU_Lib_Xcursor *xcursor);
void maru_unload_xcursor_symbols(MARU_Lib_Xcursor *xcursor);
bool maru_load_xi2_symbols(struct MARU_Context_Base *ctx, MARU_Lib_Xi2 *xi2);
void maru_unload_xi2_symbols(MARU_Lib_Xi2 *xi2);
bool maru_load_xshape_symbols(struct MARU_Context_Base *ctx, MARU_Lib_Xshape *xshape);
void maru_unload_xshape_symbols(MARU_Lib_Xshape *xshape);
bool maru_load_xrandr_symbols(struct MARU_Context_Base *ctx, MARU_Lib_Xrandr *xrandr);
void maru_unload_xrandr_symbols(MARU_Lib_Xrandr *xrandr);
bool maru_load_xfixes_symbols(struct MARU_Context_Base *ctx, MARU_Lib_Xfixes *xfixes);
void maru_unload_xfixes_symbols(MARU_Lib_Xfixes *xfixes);

#endif
