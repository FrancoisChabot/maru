// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifndef MARU_INDIRECT_BACKEND
#define MARU_INDIRECT_BACKEND
#endif

#ifndef MARU_ENABLE_BACKEND_WAYLAND
#define MARU_ENABLE_BACKEND_WAYLAND
#endif

#ifndef MARU_ENABLE_BACKEND_X11
#define MARU_ENABLE_BACKEND_X11
#endif

// Core
#include "../core/core.c"
#include "../core/core_event_queue.c"
#include "../core/core_indirect_entry.c"
#include "../core/maru_queue.c"

// Linux Common
#include "../core/linux/linux_entry.c"
#include "../core/linux/linux_context.c"
#include "../core/linux/linux_common.c"
#include "../core/linux/linux_input.c"
#include "../core/linux/linux_dataexchange.c"
#include "../core/linux/dlib/linux_loader.c"

// Wayland Backend (as Indirect)
#include "../core/linux/wayland/wayland.c"
#include "../core/linux/wayland/wayland_context.c"
#include "../core/linux/wayland/wayland_cursor.c"
#include "../core/linux/wayland/wayland_image.c"
#include "../core/linux/wayland/wayland_controller.c"
#include "../core/linux/wayland/wayland_input.c"
#include "../core/linux/wayland/wayland_monitor.c"
#include "../core/linux/wayland/wayland_window.c"
#include "../core/linux/wayland/wayland_ssd.c"
#include "../core/linux/wayland/wayland_csd.c"
#include "../core/linux/wayland/wayland_misc.c"
#include "../core/linux/wayland/wayland_dataexchange.c"
#include "../core/linux/wayland/dlib/loader.c"
#include "../core/linux/wayland/protocols/maru_protocols.c"
#include "../core/linux/wayland/wayland_vulkan.c"

// X11 Backend (as Indirect)
#include "../core/linux/x11/x11_entry.c"
#include "../core/linux/x11/x11_context.c"
#include "../core/linux/x11/x11_window.c"
#include "../core/linux/x11/x11_input.c"
#include "../core/linux/x11/x11_dataexchange.c"
#include "../core/linux/x11/x11_monitor.c"
#include "../core/linux/x11/x11_cursor.c"
#include "../core/linux/x11/x11_vulkan.c"
#include "../core/linux/x11/dlib/loader.c"
