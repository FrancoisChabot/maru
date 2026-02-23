// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_WAYLAND_PROTOCOLS_H_INCLUDED
#define MARU_WAYLAND_PROTOCOLS_H_INCLUDED

// Necessary so that the API calls get rerouted to our dynamic table
#include "dlib/wayland-client.h"

#include "generated/wayland-client-protocol.h"
#include "generated/xdg-shell-client-protocol.h"
#include "generated/xdg-decoration-unstable-v1-client-protocol.h"
#include "generated/xdg-output-unstable-v1-client-protocol.h"
#include "generated/viewporter-client-protocol.h"
#include "generated/fractional-scale-v1-client-protocol.h"
#include "generated/cursor-shape-v1-client-protocol.h"
#include "generated/text-input-unstable-v3-client-protocol.h"
#include "generated/relative-pointer-unstable-v1-client-protocol.h"
#include "generated/pointer-constraints-unstable-v1-client-protocol.h"
#include "generated/idle-inhibit-unstable-v1-client-protocol.h"
#include "generated/ext-idle-notify-v1-client-protocol.h"
#include "generated/xdg-activation-v1-client-protocol.h"
#include "generated/primary-selection-unstable-v1-client-protocol.h"
#include "generated/content-type-v1-client-protocol.h"

extern const struct xdg_wm_base_listener _maru_xdg_wm_base_listener;
extern const struct wl_seat_listener _maru_wayland_seat_listener;

// Required interfaces
#define MARU_WL_REGISTRY_REQUIRED_BINDINGS                                 \
  MARU_WL_REGISTRY_BINDING_ENTRY(xdg_wm_base, 1, &_maru_xdg_wm_base_listener) \
  MARU_WL_REGISTRY_BINDING_ENTRY(wl_compositor, 6, NULL)                   \
  MARU_WL_REGISTRY_BINDING_ENTRY(wl_shm, 1, NULL)                          \
  

// Optional interfaces
#define MARU_WL_REGISTRY_OPTIONAL_BINDINGS                                 \
  MARU_WL_REGISTRY_BINDING_ENTRY(wl_seat, 5, &_maru_wayland_seat_listener)  \
  MARU_WL_REGISTRY_BINDING_ENTRY(zxdg_output_manager_v1, 3, NULL)          \
  MARU_WL_REGISTRY_BINDING_ENTRY(zxdg_decoration_manager_v1, 1, NULL)       \
  MARU_WL_REGISTRY_BINDING_ENTRY(wp_fractional_scale_manager_v1, 1, NULL)   \
  MARU_WL_REGISTRY_BINDING_ENTRY(zwp_relative_pointer_manager_v1, 1, NULL)  \
  MARU_WL_REGISTRY_BINDING_ENTRY(zwp_pointer_constraints_v1, 1, NULL)       \

#define MARU_WL_REGISTRY_BINDING_ENTRY(name, version, listener) struct name *name;
typedef struct MARU_Wayland_Protocols_WL {
  MARU_WL_REGISTRY_REQUIRED_BINDINGS
  struct {
    MARU_WL_REGISTRY_OPTIONAL_BINDINGS
  } opt;
} MARU_Wayland_Protocols_WL;
#undef MARU_WL_REGISTRY_BINDING_ENTRY

#endif
