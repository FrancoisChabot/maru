// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WAYLAND_INTERNAL_H_INCLUDED
#define MARU_WAYLAND_INTERNAL_H_INCLUDED

#include "maru_internal.h"
#include "dlib/wayland-client.h"
#include "dlib/wayland-cursor.h"
#include "dlib/libdecor.h"
#include "protocols/maru_protocols.h"

typedef struct MARU_Context_WL {
  MARU_Context_Base base;

  struct {
    struct wl_display *display;
  } wl;

  MARU_Wayland_Protocols_WL protocols;

  struct {
    MARU_Lib_WaylandClient wl;
    MARU_Lib_WaylandCursor wlc;
    struct {
      MARU_Lib_Decor decor;
    } opt;
  } dlib;
} MARU_Context_WL;

/* wayland-client core helpers */

static inline struct wl_display *
maru_wl_display_connect(MARU_Context_WL *ctx, const char *name)
{
	return ctx->dlib.wl.display_connect(name);
}

static inline void
maru_wl_display_disconnect(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	ctx->dlib.wl.display_disconnect(wl_display);
}

static inline int
maru_wl_display_roundtrip(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_roundtrip(wl_display);
}

static inline int
maru_wl_display_dispatch_pending(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_dispatch_pending(wl_display);
}

static inline int
maru_wl_display_get_error(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_get_error(wl_display);
}

static inline int
maru_wl_display_get_fd(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_get_fd(wl_display);
}

static inline int
maru_wl_display_flush(MARU_Context_WL *ctx, struct wl_display *wl_display)
{
	return ctx->dlib.wl.display_flush(wl_display);
}

#include "protocols/generated/maru-wayland-helpers.h"
#include "protocols/generated/maru-xdg-shell-helpers.h"
#include "protocols/generated/maru-xdg-decoration-unstable-v1-helpers.h"

#endif
