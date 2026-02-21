/* Generated from xdg-activation-v1.xml */

#ifndef MARU_WAYLAND_HELPERS_XDG_ACTIVATION_V1_H
#define MARU_WAYLAND_HELPERS_XDG_ACTIVATION_V1_H

#include <stdint.h>
#include <stddef.h>

typedef struct MARU_Context_WL MARU_Context_WL;

struct xdg_activation_v1;
struct xdg_activation_v1_listener;
struct xdg_activation_token_v1;
struct wl_surface;
/* interface xdg_activation_v1 */
static inline void
maru_xdg_activation_v1_set_user_data(MARU_Context_WL *ctx, struct xdg_activation_v1 *xdg_activation_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) xdg_activation_v1, user_data);
}

static inline void *
maru_xdg_activation_v1_get_user_data(MARU_Context_WL *ctx, struct xdg_activation_v1 *xdg_activation_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) xdg_activation_v1);
}

static inline uint32_t
maru_xdg_activation_v1_get_version(MARU_Context_WL *ctx, struct xdg_activation_v1 *xdg_activation_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_v1);
}

static inline int
maru_xdg_activation_v1_add_listener(MARU_Context_WL *ctx, struct xdg_activation_v1 *xdg_activation_v1, const struct xdg_activation_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) xdg_activation_v1, (void (**)(void)) listener, data);
}

static inline void
maru_xdg_activation_v1_destroy(MARU_Context_WL *ctx, struct xdg_activation_v1 *xdg_activation_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_activation_v1, XDG_ACTIVATION_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_v1), WL_MARSHAL_FLAG_DESTROY);
}

static inline struct xdg_activation_token_v1 *
maru_xdg_activation_v1_get_activation_token(MARU_Context_WL *ctx, struct xdg_activation_v1 *xdg_activation_v1)
{
	struct wl_proxy *id;
	id = ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_activation_v1, XDG_ACTIVATION_V1_GET_ACTIVATION_TOKEN, &xdg_activation_token_v1_interface, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_v1), 0, NULL);
	return (struct xdg_activation_token_v1 *) id;
}

static inline void
maru_xdg_activation_v1_activate(MARU_Context_WL *ctx, struct xdg_activation_v1 *xdg_activation_v1, const char *token, struct wl_surface *surface)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_activation_v1, XDG_ACTIVATION_V1_ACTIVATE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_v1), 0, token, surface);
}

struct xdg_activation_token_v1;
struct xdg_activation_token_v1_listener;
struct wl_seat;
struct wl_surface;
/* interface xdg_activation_token_v1 */
static inline void
maru_xdg_activation_token_v1_set_user_data(MARU_Context_WL *ctx, struct xdg_activation_token_v1 *xdg_activation_token_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) xdg_activation_token_v1, user_data);
}

static inline void *
maru_xdg_activation_token_v1_get_user_data(MARU_Context_WL *ctx, struct xdg_activation_token_v1 *xdg_activation_token_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) xdg_activation_token_v1);
}

static inline uint32_t
maru_xdg_activation_token_v1_get_version(MARU_Context_WL *ctx, struct xdg_activation_token_v1 *xdg_activation_token_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_token_v1);
}

static inline int
maru_xdg_activation_token_v1_add_listener(MARU_Context_WL *ctx, struct xdg_activation_token_v1 *xdg_activation_token_v1, const struct xdg_activation_token_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) xdg_activation_token_v1, (void (**)(void)) listener, data);
}

static inline void
maru_xdg_activation_token_v1_set_serial(MARU_Context_WL *ctx, struct xdg_activation_token_v1 *xdg_activation_token_v1, uint32_t serial, struct wl_seat *seat)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_activation_token_v1, XDG_ACTIVATION_TOKEN_V1_SET_SERIAL, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_token_v1), 0, serial, seat);
}

static inline void
maru_xdg_activation_token_v1_set_app_id(MARU_Context_WL *ctx, struct xdg_activation_token_v1 *xdg_activation_token_v1, const char *app_id)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_activation_token_v1, XDG_ACTIVATION_TOKEN_V1_SET_APP_ID, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_token_v1), 0, app_id);
}

static inline void
maru_xdg_activation_token_v1_set_surface(MARU_Context_WL *ctx, struct xdg_activation_token_v1 *xdg_activation_token_v1, struct wl_surface *surface)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_activation_token_v1, XDG_ACTIVATION_TOKEN_V1_SET_SURFACE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_token_v1), 0, surface);
}

static inline void
maru_xdg_activation_token_v1_commit(MARU_Context_WL *ctx, struct xdg_activation_token_v1 *xdg_activation_token_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_activation_token_v1, XDG_ACTIVATION_TOKEN_V1_COMMIT, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_token_v1), 0);
}

static inline void
maru_xdg_activation_token_v1_destroy(MARU_Context_WL *ctx, struct xdg_activation_token_v1 *xdg_activation_token_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) xdg_activation_token_v1, XDG_ACTIVATION_TOKEN_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) xdg_activation_token_v1), WL_MARSHAL_FLAG_DESTROY);
}

#endif
