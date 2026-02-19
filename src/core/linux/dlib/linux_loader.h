// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_LINUX_DLIB_LOADER_H_INCLUDED
#define MARU_LINUX_DLIB_LOADER_H_INCLUDED

#include <stdbool.h>
#include "xkbcommon.h"

struct MARU_Context_Base;

bool maru_linux_xkb_load(struct MARU_Context_Base *ctx, MARU_Lib_Xkb *out_lib);
void maru_linux_xkb_unload(MARU_Lib_Xkb *lib);

#endif
