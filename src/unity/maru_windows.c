// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_ENABLE_BACKEND_WINDOWS
#define MARU_ENABLE_BACKEND_WINDOWS
#endif
// Core
#include "../core/core.c"
#include "../core/internal_event_queue.c"
#include "../core/maru_queue.c"

// Windows Backend
#include "../core/windows/windows_entry.c"
#include "../core/windows/windows_contexts.c"
#include "../core/windows/windows_windows.c"
#include "../core/windows/windows_images.c"
#include "../core/windows/windows_events.c"
#include "../core/windows/windows_controllers.c"
#include "../core/windows/windows_data_exchange.c"
#include "../core/windows/windows_cursors.c"
#include "../core/windows/windows_monitors.c"
#include "../core/windows/windows_vulkan.c"
