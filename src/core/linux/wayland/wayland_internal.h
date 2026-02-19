// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WAYLAND_INTERNAL_H_INCLUDED
#define MARU_WAYLAND_INTERNAL_H_INCLUDED

#include "linux_internal.h"
#include "dlib/wayland-client.h"
#include "dlib/wayland-cursor.h"
#include "dlib/libdecor.h"
#include "protocols/maru_protocols.h"

typedef struct MARU_Context_WL {
  MARU_Context_Base base;
  MARU_Context_Linux_Common linux_common;

  struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_cursor_theme *cursor_theme;
    struct wl_surface *cursor_surface;
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

/* wayland-cursor helpers */

static inline struct wl_cursor_theme *maru_wl_cursor_theme_load(const MARU_Context_WL *ctx,
                                                                const char *name, int size,
                                                                struct wl_shm *shm) {
  return ctx->dlib.wlc.theme_load(name, size, shm);
}

static inline void maru_wl_cursor_theme_destroy(const MARU_Context_WL *ctx,
                                                struct wl_cursor_theme *theme) {
  ctx->dlib.wlc.theme_destroy(theme);
}

/* xkbcommon helpers */

static inline struct xkb_context *maru_xkb_context_new(const MARU_Context_WL *ctx,
                                                       enum xkb_context_flags flags) {
  return ctx->linux_common.xkb_lib.context_new(flags);
}

static inline void maru_xkb_context_unref(const MARU_Context_WL *ctx, struct xkb_context *context) {
  ctx->linux_common.xkb_lib.context_unref(context);
}

MARU_Status maru_createContext_WL(const MARU_ContextCreateInfo *create_info,
                                  MARU_Context **out_context);
MARU_Status maru_destroyContext_WL(MARU_Context *context);
MARU_Status maru_pumpEvents_WL(MARU_Context *context, uint32_t timeout_ms);
MARU_Status maru_createWindow_WL(MARU_Context *context,
                                const MARU_WindowCreateInfo *create_info,
                                MARU_Window **out_window);
MARU_Status maru_destroyWindow_WL(MARU_Window *window);

#include "protocols/generated/maru-wayland-helpers.h"
#include "protocols/generated/maru-xdg-shell-helpers.h"
#include "protocols/generated/maru-xdg-decoration-unstable-v1-helpers.h"

#endif
