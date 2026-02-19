// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WAYLAND_DLIB_LOADER_H_INCLUDED
#define MARU_WAYLAND_DLIB_LOADER_H_INCLUDED

#include <stdbool.h>

#include "wayland-client.h"
#include "wayland-cursor.h"
#include "libdecor.h"

struct MARU_Context_Base;

bool maru_load_wayland_symbols(struct MARU_Context_Base *ctx, 
                               MARU_Lib_WaylandClient *wl, 
                               MARU_Lib_WaylandCursor *wlc, 
                               MARU_Lib_Decor *decor);

void maru_unload_wayland_symbols(MARU_Lib_WaylandClient *wl, 
                                 MARU_Lib_WaylandCursor *wlc, 
                                 MARU_Lib_Decor *decor);

#endif
