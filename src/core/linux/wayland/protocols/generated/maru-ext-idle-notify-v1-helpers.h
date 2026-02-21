/* Generated from ext-idle-notify-v1.xml */

#ifndef MARU_WAYLAND_HELPERS_EXT_IDLE_NOTIFY_V1_H
#define MARU_WAYLAND_HELPERS_EXT_IDLE_NOTIFY_V1_H

#include <stdint.h>
#include <stddef.h>

typedef struct MARU_Context_WL MARU_Context_WL;

struct ext_idle_notifier_v1;
struct ext_idle_notifier_v1_listener;
struct ext_idle_notification_v1;
struct wl_seat;
/* interface ext_idle_notifier_v1 */
static inline void
maru_ext_idle_notifier_v1_set_user_data(MARU_Context_WL *ctx, struct ext_idle_notifier_v1 *ext_idle_notifier_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) ext_idle_notifier_v1, user_data);
}

static inline void *
maru_ext_idle_notifier_v1_get_user_data(MARU_Context_WL *ctx, struct ext_idle_notifier_v1 *ext_idle_notifier_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) ext_idle_notifier_v1);
}

static inline uint32_t
maru_ext_idle_notifier_v1_get_version(MARU_Context_WL *ctx, struct ext_idle_notifier_v1 *ext_idle_notifier_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) ext_idle_notifier_v1);
}

static inline int
maru_ext_idle_notifier_v1_add_listener(MARU_Context_WL *ctx, struct ext_idle_notifier_v1 *ext_idle_notifier_v1, const struct ext_idle_notifier_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) ext_idle_notifier_v1, (void (**)(void)) listener, data);
}

static inline void
maru_ext_idle_notifier_v1_destroy(MARU_Context_WL *ctx, struct ext_idle_notifier_v1 *ext_idle_notifier_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) ext_idle_notifier_v1, EXT_IDLE_NOTIFIER_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) ext_idle_notifier_v1), WL_MARSHAL_FLAG_DESTROY);
}

static inline struct ext_idle_notification_v1 *
maru_ext_idle_notifier_v1_get_idle_notification(MARU_Context_WL *ctx, struct ext_idle_notifier_v1 *ext_idle_notifier_v1, uint32_t timeout, struct wl_seat *seat)
{
	struct wl_proxy *id;
	id = ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) ext_idle_notifier_v1, EXT_IDLE_NOTIFIER_V1_GET_IDLE_NOTIFICATION, &ext_idle_notification_v1_interface, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) ext_idle_notifier_v1), 0, NULL, timeout, seat);
	return (struct ext_idle_notification_v1 *) id;
}

struct ext_idle_notification_v1;
struct ext_idle_notification_v1_listener;
/* interface ext_idle_notification_v1 */
static inline void
maru_ext_idle_notification_v1_set_user_data(MARU_Context_WL *ctx, struct ext_idle_notification_v1 *ext_idle_notification_v1, void *user_data)
{
	ctx->dlib.wl.proxy_set_user_data((struct wl_proxy *) ext_idle_notification_v1, user_data);
}

static inline void *
maru_ext_idle_notification_v1_get_user_data(MARU_Context_WL *ctx, struct ext_idle_notification_v1 *ext_idle_notification_v1)
{
	return ctx->dlib.wl.proxy_get_user_data((struct wl_proxy *) ext_idle_notification_v1);
}

static inline uint32_t
maru_ext_idle_notification_v1_get_version(MARU_Context_WL *ctx, struct ext_idle_notification_v1 *ext_idle_notification_v1)
{
	return ctx->dlib.wl.proxy_get_version((struct wl_proxy *) ext_idle_notification_v1);
}

static inline int
maru_ext_idle_notification_v1_add_listener(MARU_Context_WL *ctx, struct ext_idle_notification_v1 *ext_idle_notification_v1, const struct ext_idle_notification_v1_listener *listener, void *data)
{
	return ctx->dlib.wl.proxy_add_listener((struct wl_proxy *) ext_idle_notification_v1, (void (**)(void)) listener, data);
}

static inline void
maru_ext_idle_notification_v1_destroy(MARU_Context_WL *ctx, struct ext_idle_notification_v1 *ext_idle_notification_v1)
{
	ctx->dlib.wl.proxy_marshal_flags((struct wl_proxy *) ext_idle_notification_v1, EXT_IDLE_NOTIFICATION_V1_DESTROY, NULL, ctx->dlib.wl.proxy_get_version((struct wl_proxy *) ext_idle_notification_v1), WL_MARSHAL_FLAG_DESTROY);
}

#endif
