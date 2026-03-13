// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_ENABLE_BACKEND_WAYLAND
#define MARU_ENABLE_BACKEND_WAYLAND
#endif

// Core
#include "../core/core.c"
#include "../core/core_event_queue.c"
#include "../core/maru_queue.c"

// Linux Common
#include "../core/linux/linux_entry.c"
#include "../core/linux/linux_context.c"
#include "../core/linux/linux_common.c"
#include "../core/linux/linux_dataexchange.c"
#include "../core/linux/dlib/linux_loader.c"

// Wayland Backend
#include "../core/linux/wayland/wayland.c"
#include "../core/linux/wayland/wayland_context.c"
#include "../core/linux/wayland/wayland_image.c"
#include "../core/linux/wayland/wayland_controller.c"
#include "../core/linux/wayland/wayland_input.c"
#include "../core/linux/wayland/wayland_monitor.c"
#include "../core/linux/wayland/wayland_window.c"
#include "../core/linux/wayland/wayland_ssd.c"
#include "../core/linux/wayland/wayland_csd.c"
#include "../core/linux/wayland/wayland_misc.c"
#include "../core/linux/wayland/wayland_dataexchange.c"
#include "../core/linux/wayland/wayland_utils.c"
#include "../core/linux/wayland/dlib/loader.c"
#include "../core/linux/wayland/protocols/maru_protocols.c"
#include "../core/linux/wayland/wayland_vulkan.c"
