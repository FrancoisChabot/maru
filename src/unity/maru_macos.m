// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_ENABLE_BACKEND_COCOA
#define MARU_ENABLE_BACKEND_COCOA
#endif

// Core
#include "../core/core.c"
#include "../core/core_event_queue.c"
#include "../core/maru_queue.c"

// macOS Backend
#import "../core/macos/macos_entry.m"
#import "../core/macos/macos_context.m"
#import "../core/macos/macos_window.m"
#import "../core/macos/macos_cursor.m"
#import "../core/macos/macos_image.m"
#import "../core/macos/macos_controller.m"
#import "../core/macos/macos_dataexchange.m"
#import "../core/macos/macos_monitor.m"
#import "../core/macos/macos_vulkan.m"
