/* Generated from xdg-shell.xml */

#ifndef MARU_WAYLAND_HELPERS_XDG_SHELL_H
#define MARU_WAYLAND_HELPERS_XDG_SHELL_H

#include <stdint.h>
#include <stddef.h>

typedef struct MARU_Context_WL MARU_Context_WL;

struct xdg_wm_base;
struct xdg_wm_base_listener;
struct xdg_positioner;
struct xdg_surface;
struct wl_surface;
/* interface xdg_wm_base */
static inline void
maru_xdg_wm_base_set_user_data(MARU_Context_WL *ctx, struct xdg_wm_base *xdg_wm_base, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) xdg_wm_base, user_data);
}

static inline void *
maru_xdg_wm_base_get_user_data(MARU_Context_WL *ctx, struct xdg_wm_base *xdg_wm_base)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) xdg_wm_base);
}

static inline uint32_t
maru_xdg_wm_base_get_version(MARU_Context_WL *ctx, struct xdg_wm_base *xdg_wm_base)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_wm_base);
}

static inline int
maru_xdg_wm_base_add_listener(MARU_Context_WL *ctx, struct xdg_wm_base *xdg_wm_base, const struct xdg_wm_base_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) xdg_wm_base, (void (**)(void)) listener, data);
}

static inline void
maru_xdg_wm_base_destroy(MARU_Context_WL *ctx, struct xdg_wm_base *xdg_wm_base)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_wm_base, XDG_WM_BASE_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_wm_base), WL_MARSHAL_FLAG_DESTROY);
}

static inline struct xdg_positioner *
maru_xdg_wm_base_create_positioner(MARU_Context_WL *ctx, struct xdg_wm_base *xdg_wm_base)
{
	struct wl_proxy *id;
	id = ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_wm_base, XDG_WM_BASE_CREATE_POSITIONER, &xdg_positioner_interface, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_wm_base), 0, NULL);
	return (struct xdg_positioner *) id;
}

static inline struct xdg_surface *
maru_xdg_wm_base_get_xdg_surface(MARU_Context_WL *ctx, struct xdg_wm_base *xdg_wm_base, struct wl_surface *surface)
{
	struct wl_proxy *id;
	id = ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_wm_base, XDG_WM_BASE_GET_XDG_SURFACE, &xdg_surface_interface, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_wm_base), 0, NULL, surface);
	return (struct xdg_surface *) id;
}

static inline void
maru_xdg_wm_base_pong(MARU_Context_WL *ctx, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_wm_base, XDG_WM_BASE_PONG, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_wm_base), 0, serial);
}

struct xdg_positioner;
struct xdg_positioner_listener;
/* interface xdg_positioner */
static inline void
maru_xdg_positioner_set_user_data(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) xdg_positioner, user_data);
}

static inline void *
maru_xdg_positioner_get_user_data(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) xdg_positioner);
}

static inline uint32_t
maru_xdg_positioner_get_version(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner);
}

static inline int
maru_xdg_positioner_add_listener(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, const struct xdg_positioner_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) xdg_positioner, (void (**)(void)) listener, data);
}

static inline void
maru_xdg_positioner_destroy(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), WL_MARSHAL_FLAG_DESTROY);
}

static inline void
maru_xdg_positioner_set_size(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, int32_t width, int32_t height)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_SET_SIZE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), 0, width, height);
}

static inline void
maru_xdg_positioner_set_anchor_rect(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, int32_t x, int32_t y, int32_t width, int32_t height)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_SET_ANCHOR_RECT, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), 0, x, y, width, height);
}

static inline void
maru_xdg_positioner_set_anchor(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, uint32_t anchor)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_SET_ANCHOR, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), 0, anchor);
}

static inline void
maru_xdg_positioner_set_gravity(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, uint32_t gravity)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_SET_GRAVITY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), 0, gravity);
}

static inline void
maru_xdg_positioner_set_constraint_adjustment(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, uint32_t constraint_adjustment)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_SET_CONSTRAINT_ADJUSTMENT, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), 0, constraint_adjustment);
}

static inline void
maru_xdg_positioner_set_offset(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, int32_t x, int32_t y)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_SET_OFFSET, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), 0, x, y);
}

static inline void
maru_xdg_positioner_set_reactive(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_SET_REACTIVE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), 0);
}

static inline void
maru_xdg_positioner_set_parent_size(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, int32_t parent_width, int32_t parent_height)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_SET_PARENT_SIZE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), 0, parent_width, parent_height);
}

static inline void
maru_xdg_positioner_set_parent_configure(MARU_Context_WL *ctx, struct xdg_positioner *xdg_positioner, uint32_t serial)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_positioner, XDG_POSITIONER_SET_PARENT_CONFIGURE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_positioner), 0, serial);
}

struct xdg_surface;
struct xdg_surface_listener;
struct xdg_toplevel;
struct xdg_popup;
struct xdg_surface;
struct xdg_positioner;
/* interface xdg_surface */
static inline void
maru_xdg_surface_set_user_data(MARU_Context_WL *ctx, struct xdg_surface *xdg_surface, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) xdg_surface, user_data);
}

static inline void *
maru_xdg_surface_get_user_data(MARU_Context_WL *ctx, struct xdg_surface *xdg_surface)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) xdg_surface);
}

static inline uint32_t
maru_xdg_surface_get_version(MARU_Context_WL *ctx, struct xdg_surface *xdg_surface)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_surface);
}

static inline int
maru_xdg_surface_add_listener(MARU_Context_WL *ctx, struct xdg_surface *xdg_surface, const struct xdg_surface_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) xdg_surface, (void (**)(void)) listener, data);
}

static inline void
maru_xdg_surface_destroy(MARU_Context_WL *ctx, struct xdg_surface *xdg_surface)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_surface, XDG_SURFACE_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_surface), WL_MARSHAL_FLAG_DESTROY);
}

static inline struct xdg_toplevel *
maru_xdg_surface_get_toplevel(MARU_Context_WL *ctx, struct xdg_surface *xdg_surface)
{
	struct wl_proxy *id;
	id = ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_surface, XDG_SURFACE_GET_TOPLEVEL, &xdg_toplevel_interface, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_surface), 0, NULL);
	return (struct xdg_toplevel *) id;
}

static inline struct xdg_popup *
maru_xdg_surface_get_popup(MARU_Context_WL *ctx, struct xdg_surface *xdg_surface, struct xdg_surface *parent, struct xdg_positioner *positioner)
{
	struct wl_proxy *id;
	id = ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_surface, XDG_SURFACE_GET_POPUP, &xdg_popup_interface, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_surface), 0, NULL, parent, positioner);
	return (struct xdg_popup *) id;
}

static inline void
maru_xdg_surface_set_window_geometry(MARU_Context_WL *ctx, struct xdg_surface *xdg_surface, int32_t x, int32_t y, int32_t width, int32_t height)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_surface, XDG_SURFACE_SET_WINDOW_GEOMETRY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_surface), 0, x, y, width, height);
}

static inline void
maru_xdg_surface_ack_configure(MARU_Context_WL *ctx, struct xdg_surface *xdg_surface, uint32_t serial)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_surface, XDG_SURFACE_ACK_CONFIGURE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_surface), 0, serial);
}

struct xdg_toplevel;
struct xdg_toplevel_listener;
struct xdg_toplevel;
struct wl_seat;
struct wl_seat;
struct wl_seat;
struct wl_output;
/* interface xdg_toplevel */
static inline void
maru_xdg_toplevel_set_user_data(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) xdg_toplevel, user_data);
}

static inline void *
maru_xdg_toplevel_get_user_data(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) xdg_toplevel);
}

static inline uint32_t
maru_xdg_toplevel_get_version(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel);
}

static inline int
maru_xdg_toplevel_add_listener(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, const struct xdg_toplevel_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) xdg_toplevel, (void (**)(void)) listener, data);
}

static inline void
maru_xdg_toplevel_destroy(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), WL_MARSHAL_FLAG_DESTROY);
}

static inline void
maru_xdg_toplevel_set_parent(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, struct xdg_toplevel *parent)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_SET_PARENT, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, parent);
}

static inline void
maru_xdg_toplevel_set_title(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, const char *title)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_SET_TITLE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, title);
}

static inline void
maru_xdg_toplevel_set_app_id(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, const char *app_id)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_SET_APP_ID, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, app_id);
}

static inline void
maru_xdg_toplevel_show_window_menu(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial, int32_t x, int32_t y)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_SHOW_WINDOW_MENU, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, seat, serial, x, y);
}

static inline void
maru_xdg_toplevel_move(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_MOVE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, seat, serial);
}

static inline void
maru_xdg_toplevel_resize(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, struct wl_seat *seat, uint32_t serial, uint32_t edges)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_RESIZE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, seat, serial, edges);
}

static inline void
maru_xdg_toplevel_set_max_size(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_SET_MAX_SIZE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, width, height);
}

static inline void
maru_xdg_toplevel_set_min_size(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_SET_MIN_SIZE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, width, height);
}

static inline void
maru_xdg_toplevel_set_maximized(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_SET_MAXIMIZED, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0);
}

static inline void
maru_xdg_toplevel_unset_maximized(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_UNSET_MAXIMIZED, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0);
}

static inline void
maru_xdg_toplevel_set_fullscreen(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel, struct wl_output *output)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_SET_FULLSCREEN, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, output);
}

static inline void
maru_xdg_toplevel_unset_fullscreen(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_UNSET_FULLSCREEN, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0);
}

static inline void
maru_xdg_toplevel_set_minimized(MARU_Context_WL *ctx, struct xdg_toplevel *xdg_toplevel)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_toplevel, XDG_TOPLEVEL_SET_MINIMIZED, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_toplevel), 0);
}

struct xdg_popup;
struct xdg_popup_listener;
struct wl_seat;
struct xdg_positioner;
/* interface xdg_popup */
static inline void
maru_xdg_popup_set_user_data(MARU_Context_WL *ctx, struct xdg_popup *xdg_popup, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) xdg_popup, user_data);
}

static inline void *
maru_xdg_popup_get_user_data(MARU_Context_WL *ctx, struct xdg_popup *xdg_popup)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) xdg_popup);
}

static inline uint32_t
maru_xdg_popup_get_version(MARU_Context_WL *ctx, struct xdg_popup *xdg_popup)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_popup);
}

static inline int
maru_xdg_popup_add_listener(MARU_Context_WL *ctx, struct xdg_popup *xdg_popup, const struct xdg_popup_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) xdg_popup, (void (**)(void)) listener, data);
}

static inline void
maru_xdg_popup_destroy(MARU_Context_WL *ctx, struct xdg_popup *xdg_popup)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_popup, XDG_POPUP_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_popup), WL_MARSHAL_FLAG_DESTROY);
}

static inline void
maru_xdg_popup_grab(MARU_Context_WL *ctx, struct xdg_popup *xdg_popup, struct wl_seat *seat, uint32_t serial)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_popup, XDG_POPUP_GRAB, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_popup), 0, seat, serial);
}

static inline void
maru_xdg_popup_reposition(MARU_Context_WL *ctx, struct xdg_popup *xdg_popup, struct xdg_positioner *positioner, uint32_t token)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_popup, XDG_POPUP_REPOSITION, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_popup), 0, positioner, token);
}

#endif
