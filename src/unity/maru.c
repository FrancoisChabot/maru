// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#if defined(__linux__)
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#endif

// 1. Sanitize backends based on platform
#if defined(_WIN32)
#  undef MARU_ENABLE_BACKEND_WAYLAND
#  undef MARU_ENABLE_BACKEND_X11
#  undef MARU_ENABLE_BACKEND_COCOA
#  ifndef MARU_ENABLE_BACKEND_WINDOWS
#    define MARU_ENABLE_BACKEND_WINDOWS
#  endif
#elif defined(__APPLE__)
#  undef MARU_ENABLE_BACKEND_WAYLAND
#  undef MARU_ENABLE_BACKEND_X11
#  undef MARU_ENABLE_BACKEND_WINDOWS
#  ifndef MARU_ENABLE_BACKEND_COCOA
#    define MARU_ENABLE_BACKEND_COCOA
#  endif
#elif defined(__linux__)
#  undef MARU_ENABLE_BACKEND_WINDOWS
#  undef MARU_ENABLE_BACKEND_COCOA
#  if !defined(MARU_ENABLE_BACKEND_WAYLAND) && !defined(MARU_ENABLE_BACKEND_X11)
#    define MARU_ENABLE_BACKEND_WAYLAND
#    define MARU_ENABLE_BACKEND_X11
#  endif
#endif

#include "maru/details/maru_config.h"

// 2. Determine Indirect
// We use indirect mode by default, unless on Linux where only one backend is enabled.
#if defined(__linux__)
#  if defined(MARU_ENABLE_BACKEND_WAYLAND) && defined(MARU_ENABLE_BACKEND_X11)
#    ifndef MARU_INDIRECT_BACKEND
#      define MARU_INDIRECT_BACKEND
#    endif
#  else
#    undef MARU_INDIRECT_BACKEND
#  endif
#else
#  ifndef MARU_INDIRECT_BACKEND
#    define MARU_INDIRECT_BACKEND
#  endif
#endif

// Core files
#include "../core/core.c"
#include "../core/internal_event_queue.c"
#include "../core/maru_queue.c"

#ifdef MARU_INDIRECT_BACKEND
#include "../core/core_indirect_entry.c"
#endif

#if defined(MARU_ENABLE_BACKEND_WINDOWS)
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

#elif defined(MARU_ENABLE_BACKEND_COCOA)
#import "../core/macos/macos_entry.m"
#import "../core/macos/macos_context.m"
#import "../core/macos/macos_window.m"
#import "../core/macos/macos_cursor.m"
#import "../core/macos/macos_image.m"
#import "../core/macos/macos_controller.m"
#import "../core/macos/macos_dataexchange.m"
#import "../core/macos/macos_monitor.m"
#import "../core/macos/macos_vulkan.m"

#elif defined(__linux__)
// Linux Common
#include "../core/linux/linux_entry.c"
#include "../core/linux/linux_context.c"
#include "../core/linux/linux_common.c"
#include "../core/linux/linux_input.c"
#include "../core/linux/linux_dataexchange.c"
#include "../core/linux/dlib/linux_loader.c"

#ifdef MARU_ENABLE_BACKEND_WAYLAND
// Wayland Backend
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
#endif

#ifdef MARU_ENABLE_BACKEND_X11
// X11 Backend
#include "../core/linux/x11/x11_entry.c"
#include "../core/linux/x11/x11_context.c"
#include "../core/linux/x11/x11_window.c"
#include "../core/linux/x11/x11_input.c"
#include "../core/linux/x11/x11_dataexchange.c"
#include "../core/linux/x11/x11_monitor.c"
#include "../core/linux/x11/x11_cursor.c"
#include "../core/linux/x11/x11_vulkan.c"
#include "../core/linux/x11/dlib/loader.c"
#endif
#endif
