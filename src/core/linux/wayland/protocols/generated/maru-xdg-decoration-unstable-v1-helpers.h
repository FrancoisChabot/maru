/* Generated from xdg-decoration-unstable-v1.xml */

#ifndef MARU_WAYLAND_HELPERS_XDG_DECORATION_UNSTABLE_V1_H
#define MARU_WAYLAND_HELPERS_XDG_DECORATION_UNSTABLE_V1_H

#include <stdint.h>
#include <stddef.h>

typedef struct MARU_Context_WL MARU_Context_WL;

struct zxdg_decoration_manager_v1;
struct zxdg_decoration_manager_v1_listener;
struct zxdg_toplevel_decoration_v1;
struct xdg_toplevel;
/* interface zxdg_decoration_manager_v1 */
static inline void
maru_zxdg_decoration_manager_v1_set_user_data(MARU_Context_WL *ctx, struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) zxdg_decoration_manager_v1, user_data);
}

static inline void *
maru_zxdg_decoration_manager_v1_get_user_data(MARU_Context_WL *ctx, struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) zxdg_decoration_manager_v1);
}

static inline uint32_t
maru_zxdg_decoration_manager_v1_get_version(MARU_Context_WL *ctx, struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zxdg_decoration_manager_v1);
}

static inline int
maru_zxdg_decoration_manager_v1_add_listener(MARU_Context_WL *ctx, struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1, const struct zxdg_decoration_manager_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) zxdg_decoration_manager_v1, (void (**)(void)) listener, data);
}

static inline void
maru_zxdg_decoration_manager_v1_destroy(MARU_Context_WL *ctx, struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zxdg_decoration_manager_v1, ZXDG_DECORATION_MANAGER_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zxdg_decoration_manager_v1), WL_MARSHAL_FLAG_DESTROY);
}

static inline struct zxdg_toplevel_decoration_v1 *
maru_zxdg_decoration_manager_v1_get_toplevel_decoration(MARU_Context_WL *ctx, struct zxdg_decoration_manager_v1 *zxdg_decoration_manager_v1, struct xdg_toplevel *toplevel)
{
	struct wl_proxy *id;
	id = ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zxdg_decoration_manager_v1, ZXDG_DECORATION_MANAGER_V1_GET_TOPLEVEL_DECORATION, &zxdg_toplevel_decoration_v1_interface, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zxdg_decoration_manager_v1), 0, NULL, toplevel);
	return (struct zxdg_toplevel_decoration_v1 *) id;
}

struct zxdg_toplevel_decoration_v1;
struct zxdg_toplevel_decoration_v1_listener;
/* interface zxdg_toplevel_decoration_v1 */
static inline void
maru_zxdg_toplevel_decoration_v1_set_user_data(MARU_Context_WL *ctx, struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) zxdg_toplevel_decoration_v1, user_data);
}

static inline void *
maru_zxdg_toplevel_decoration_v1_get_user_data(MARU_Context_WL *ctx, struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) zxdg_toplevel_decoration_v1);
}

static inline uint32_t
maru_zxdg_toplevel_decoration_v1_get_version(MARU_Context_WL *ctx, struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zxdg_toplevel_decoration_v1);
}

static inline int
maru_zxdg_toplevel_decoration_v1_add_listener(MARU_Context_WL *ctx, struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1, const struct zxdg_toplevel_decoration_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) zxdg_toplevel_decoration_v1, (void (**)(void)) listener, data);
}

static inline void
maru_zxdg_toplevel_decoration_v1_destroy(MARU_Context_WL *ctx, struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zxdg_toplevel_decoration_v1, ZXDG_TOPLEVEL_DECORATION_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zxdg_toplevel_decoration_v1), WL_MARSHAL_FLAG_DESTROY);
}

static inline void
maru_zxdg_toplevel_decoration_v1_set_mode(MARU_Context_WL *ctx, struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1, uint32_t mode)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zxdg_toplevel_decoration_v1, ZXDG_TOPLEVEL_DECORATION_V1_SET_MODE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zxdg_toplevel_decoration_v1), 0, mode);
}

static inline void
maru_zxdg_toplevel_decoration_v1_unset_mode(MARU_Context_WL *ctx, struct zxdg_toplevel_decoration_v1 *zxdg_toplevel_decoration_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zxdg_toplevel_decoration_v1, ZXDG_TOPLEVEL_DECORATION_V1_UNSET_MODE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zxdg_toplevel_decoration_v1), 0);
}

#endif
