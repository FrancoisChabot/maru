// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_ENABLE_BACKEND_X11
#define MARU_ENABLE_BACKEND_X11
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

// X11 Backend
#include "../core/linux/x11/x11_entry.c"
#include "../core/linux/x11/x11_context.c"
#include "../core/linux/x11/x11_window.c"
#include "../core/linux/x11/x11_input.c"
#include "../core/linux/x11/x11_dataexchange.c"
#include "../core/linux/x11/x11_monitor.c"
#include "../core/linux/x11/x11_vulkan.c"
#include "../core/linux/x11/dlib/loader.c"
