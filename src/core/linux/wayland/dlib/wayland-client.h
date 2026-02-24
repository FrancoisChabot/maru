// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WAYLAND_CLIENT_DLIB_H_INCLUDED
#define MARU_WAYLAND_CLIENT_DLIB_H_INCLUDED

#include "maru_internal.h"
#include "vendor/wayland-client-core.h"

#define MARU_WL_FUNCTIONS_TABLE           \
  MARU_LIB_FN(display_connect)            \
  MARU_LIB_FN(display_disconnect)         \
  MARU_LIB_FN(display_roundtrip)          \
  MARU_LIB_FN(display_flush)              \
  MARU_LIB_FN(display_prepare_read)       \
  MARU_LIB_FN(display_get_fd)             \
  MARU_LIB_FN(display_read_events)        \
  MARU_LIB_FN(display_cancel_read)        \
  MARU_LIB_FN(display_dispatch_pending)   \
  MARU_LIB_FN(display_get_error)          \
  MARU_LIB_FN(display_get_protocol_error) \
  MARU_LIB_FN(proxy_get_version)          \
  MARU_LIB_FN(proxy_get_id)               \
  MARU_LIB_FN(proxy_marshal_flags)        \
  MARU_LIB_FN(proxy_add_listener)         \
  MARU_LIB_FN(proxy_destroy)              \
  MARU_LIB_FN(proxy_set_user_data)        \
  MARU_LIB_FN(proxy_get_user_data)

typedef struct MARU_Lib_WaylandClient {
  MARU_External_Lib_Base base;
#define MARU_LIB_FN(name) __typeof__(wl_##name) *name;
  MARU_WL_FUNCTIONS_TABLE
#undef MARU_LIB_FN
} MARU_Lib_WaylandClient;

#ifdef MARU_ENABLE_INTERNAL_CHECKS
void _maru_wayland_symbol_poison_trap(void);
#define MARU_WL_TRAP() _maru_wayland_symbol_poison_trap()

static inline void _maru_wl_trap_v(void) {
  MARU_WL_TRAP();
}
static inline int _maru_wl_trap_i(void) {
  MARU_WL_TRAP();
  return 0;
}
static inline uint32_t _maru_wl_trap_u(void) {
  MARU_WL_TRAP();
  return 0;
}
static inline void *_maru_wl_trap_p(void) {
  MARU_WL_TRAP();
  return NULL;
}

#define maru_wl_poison_v(...) _maru_wl_trap_v()
#define maru_wl_poison_i(...) _maru_wl_trap_i()
#define maru_wl_poison_u(...) _maru_wl_trap_u()
#define maru_wl_poison_p(...) _maru_wl_trap_p()

#else
#define MARU_WL_TRAP() ((void)0)

#define maru_wl_poison_v(...)
#define maru_wl_poison_i(...) 0
#define maru_wl_poison_u(...) 0
#define maru_wl_poison_p(...) NULL
#endif

#define wl_display_connect            maru_wl_poison_p
#define wl_display_disconnect         maru_wl_poison_v
#define wl_display_roundtrip          maru_wl_poison_i
#define wl_display_flush              maru_wl_poison_i
#define wl_display_prepare_read       maru_wl_poison_i
#define wl_display_get_fd             maru_wl_poison_i
#define wl_display_read_events        maru_wl_poison_i
#define wl_display_cancel_read        maru_wl_poison_v
#define wl_display_dispatch_pending   maru_wl_poison_i
#define wl_display_get_error          maru_wl_poison_i
#define wl_display_get_protocol_error maru_wl_poison_u
#define wl_proxy_get_version          maru_wl_poison_u
#define wl_proxy_get_id               maru_wl_poison_u
#define wl_proxy_marshal_flags        maru_wl_poison_p
#define wl_proxy_add_listener         maru_wl_poison_i
#define wl_proxy_destroy              maru_wl_poison_v
#define wl_proxy_set_user_data        maru_wl_poison_v
#define wl_proxy_get_user_data        maru_wl_poison_p

#include "vendor/wayland-client.h"

#endif
