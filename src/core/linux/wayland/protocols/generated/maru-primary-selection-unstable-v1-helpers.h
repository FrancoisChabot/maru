/* Generated from primary-selection-unstable-v1.xml */

#ifndef MARU_WAYLAND_HELPERS_WP_PRIMARY_SELECTION_UNSTABLE_V1_H
#define MARU_WAYLAND_HELPERS_WP_PRIMARY_SELECTION_UNSTABLE_V1_H

#include <stdint.h>
#include <stddef.h>

typedef struct MARU_Context_WL MARU_Context_WL;

struct zwp_primary_selection_device_manager_v1;
struct zwp_primary_selection_device_manager_v1_listener;
struct zwp_primary_selection_source_v1;
struct zwp_primary_selection_device_v1;
struct wl_seat;
/* interface zwp_primary_selection_device_manager_v1 */
static inline void
maru_zwp_primary_selection_device_manager_v1_set_user_data(MARU_Context_WL *ctx, struct zwp_primary_selection_device_manager_v1 *zwp_primary_selection_device_manager_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) zwp_primary_selection_device_manager_v1, user_data);
}

static inline void *
maru_zwp_primary_selection_device_manager_v1_get_user_data(MARU_Context_WL *ctx, struct zwp_primary_selection_device_manager_v1 *zwp_primary_selection_device_manager_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) zwp_primary_selection_device_manager_v1);
}

static inline uint32_t
maru_zwp_primary_selection_device_manager_v1_get_version(MARU_Context_WL *ctx, struct zwp_primary_selection_device_manager_v1 *zwp_primary_selection_device_manager_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_device_manager_v1);
}

static inline int
maru_zwp_primary_selection_device_manager_v1_add_listener(MARU_Context_WL *ctx, struct zwp_primary_selection_device_manager_v1 *zwp_primary_selection_device_manager_v1, const struct zwp_primary_selection_device_manager_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) zwp_primary_selection_device_manager_v1, (void (**)(void)) listener, data);
}

static inline struct zwp_primary_selection_source_v1 *
maru_zwp_primary_selection_device_manager_v1_create_source(MARU_Context_WL *ctx, struct zwp_primary_selection_device_manager_v1 *zwp_primary_selection_device_manager_v1)
{
	struct wl_proxy *id;
	id = ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zwp_primary_selection_device_manager_v1, ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_V1_CREATE_SOURCE, &zwp_primary_selection_source_v1_interface, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_device_manager_v1), 0, NULL);
	return (struct zwp_primary_selection_source_v1 *) id;
}

static inline struct zwp_primary_selection_device_v1 *
maru_zwp_primary_selection_device_manager_v1_get_device(MARU_Context_WL *ctx, struct zwp_primary_selection_device_manager_v1 *zwp_primary_selection_device_manager_v1, struct wl_seat *seat)
{
	struct wl_proxy *id;
	id = ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zwp_primary_selection_device_manager_v1, ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_V1_GET_DEVICE, &zwp_primary_selection_device_v1_interface, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_device_manager_v1), 0, NULL, seat);
	return (struct zwp_primary_selection_device_v1 *) id;
}

static inline void
maru_zwp_primary_selection_device_manager_v1_destroy(MARU_Context_WL *ctx, struct zwp_primary_selection_device_manager_v1 *zwp_primary_selection_device_manager_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zwp_primary_selection_device_manager_v1, ZWP_PRIMARY_SELECTION_DEVICE_MANAGER_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_device_manager_v1), WL_MARSHAL_FLAG_DESTROY);
}

struct zwp_primary_selection_device_v1;
struct zwp_primary_selection_device_v1_listener;
struct zwp_primary_selection_source_v1;
/* interface zwp_primary_selection_device_v1 */
static inline void
maru_zwp_primary_selection_device_v1_set_user_data(MARU_Context_WL *ctx, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) zwp_primary_selection_device_v1, user_data);
}

static inline void *
maru_zwp_primary_selection_device_v1_get_user_data(MARU_Context_WL *ctx, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) zwp_primary_selection_device_v1);
}

static inline uint32_t
maru_zwp_primary_selection_device_v1_get_version(MARU_Context_WL *ctx, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_device_v1);
}

static inline int
maru_zwp_primary_selection_device_v1_add_listener(MARU_Context_WL *ctx, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1, const struct zwp_primary_selection_device_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) zwp_primary_selection_device_v1, (void (**)(void)) listener, data);
}

static inline void
maru_zwp_primary_selection_device_v1_set_selection(MARU_Context_WL *ctx, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1, struct zwp_primary_selection_source_v1 *source, uint32_t serial)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zwp_primary_selection_device_v1, ZWP_PRIMARY_SELECTION_DEVICE_V1_SET_SELECTION, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_device_v1), 0, source, serial);
}

static inline void
maru_zwp_primary_selection_device_v1_destroy(MARU_Context_WL *ctx, struct zwp_primary_selection_device_v1 *zwp_primary_selection_device_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zwp_primary_selection_device_v1, ZWP_PRIMARY_SELECTION_DEVICE_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_device_v1), WL_MARSHAL_FLAG_DESTROY);
}

struct zwp_primary_selection_offer_v1;
struct zwp_primary_selection_offer_v1_listener;
/* interface zwp_primary_selection_offer_v1 */
static inline void
maru_zwp_primary_selection_offer_v1_set_user_data(MARU_Context_WL *ctx, struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) zwp_primary_selection_offer_v1, user_data);
}

static inline void *
maru_zwp_primary_selection_offer_v1_get_user_data(MARU_Context_WL *ctx, struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) zwp_primary_selection_offer_v1);
}

static inline uint32_t
maru_zwp_primary_selection_offer_v1_get_version(MARU_Context_WL *ctx, struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_offer_v1);
}

static inline int
maru_zwp_primary_selection_offer_v1_add_listener(MARU_Context_WL *ctx, struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1, const struct zwp_primary_selection_offer_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) zwp_primary_selection_offer_v1, (void (**)(void)) listener, data);
}

static inline void
maru_zwp_primary_selection_offer_v1_receive(MARU_Context_WL *ctx, struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1, const char *mime_type, int32_t fd)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zwp_primary_selection_offer_v1, ZWP_PRIMARY_SELECTION_OFFER_V1_RECEIVE, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_offer_v1), 0, mime_type, fd);
}

static inline void
maru_zwp_primary_selection_offer_v1_destroy(MARU_Context_WL *ctx, struct zwp_primary_selection_offer_v1 *zwp_primary_selection_offer_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zwp_primary_selection_offer_v1, ZWP_PRIMARY_SELECTION_OFFER_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_offer_v1), WL_MARSHAL_FLAG_DESTROY);
}

struct zwp_primary_selection_source_v1;
struct zwp_primary_selection_source_v1_listener;
/* interface zwp_primary_selection_source_v1 */
static inline void
maru_zwp_primary_selection_source_v1_set_user_data(MARU_Context_WL *ctx, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) zwp_primary_selection_source_v1, user_data);
}

static inline void *
maru_zwp_primary_selection_source_v1_get_user_data(MARU_Context_WL *ctx, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) zwp_primary_selection_source_v1);
}

static inline uint32_t
maru_zwp_primary_selection_source_v1_get_version(MARU_Context_WL *ctx, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_source_v1);
}

static inline int
maru_zwp_primary_selection_source_v1_add_listener(MARU_Context_WL *ctx, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1, const struct zwp_primary_selection_source_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) zwp_primary_selection_source_v1, (void (**)(void)) listener, data);
}

static inline void
maru_zwp_primary_selection_source_v1_offer(MARU_Context_WL *ctx, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1, const char *mime_type)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zwp_primary_selection_source_v1, ZWP_PRIMARY_SELECTION_SOURCE_V1_OFFER, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_source_v1), 0, mime_type);
}

static inline void
maru_zwp_primary_selection_source_v1_destroy(MARU_Context_WL *ctx, struct zwp_primary_selection_source_v1 *zwp_primary_selection_source_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) zwp_primary_selection_source_v1, ZWP_PRIMARY_SELECTION_SOURCE_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) zwp_primary_selection_source_v1), WL_MARSHAL_FLAG_DESTROY);
}

#endif
