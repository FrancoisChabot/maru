// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_X11_DLIB_LOADER_H_INCLUDED
#define MARU_X11_DLIB_LOADER_H_INCLUDED

#include <stdbool.h>
#include "x11.h"

struct MARU_Context_Base;

bool maru_load_x11_symbols(struct MARU_Context_Base *ctx, MARU_Lib_X11 *x11);
void maru_unload_x11_symbols(MARU_Lib_X11 *x11);

#endif
