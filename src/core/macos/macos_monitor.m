// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#import "macos_internal.h"
#import "maru_mem_internal.h"
#import <Cocoa/Cocoa.h>
#import <CoreVideo/CoreVideo.h>

typedef struct MARU_Monitor_Cocoa {
    MARU_Monitor_Base base;
    CGDirectDisplayID display_id;
    MARU_VideoMode *modes;
    uint32_t mode_count;
} MARU_Monitor_Cocoa;

static void _maru_cocoa_populate_video_modes(MARU_Monitor_Cocoa *mon) {
    if (mon->modes) return; // Already populated

    CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(mon->display_id, NULL);
    if (!displayModes) {
        mon->modes = NULL;
        mon->mode_count = 0;
        return;
    }

    CFIndex count = CFArrayGetCount(displayModes);
    mon->modes = maru_context_alloc(mon->base.ctx_base, sizeof(MARU_VideoMode) * (size_t)count);
    mon->mode_count = 0;

    for (CFIndex i = 0; i < count; i++) {
        CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);
        MARU_VideoMode *vm = &mon->modes[mon->mode_count++];
        vm->size.x = (int32_t)CGDisplayModeGetPixelWidth(mode);
        vm->size.y = (int32_t)CGDisplayModeGetPixelHeight(mode);
        vm->refresh_rate_millihz = (uint32_t)(CGDisplayModeGetRefreshRate(mode) * 1000.0);
        if (vm->refresh_rate_millihz == 0) vm->refresh_rate_millihz = 60000;
    }
    CFRelease(displayModes);
}

MARU_Status maru_getMonitors_Cocoa(MARU_Context *context, MARU_MonitorList *out_list) {
    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;

    if (ctx_base->monitor_cache) {
        out_list->monitors = ctx_base->monitor_cache;
        out_list->count = ctx_base->monitor_cache_count;
        return MARU_SUCCESS;
    }

    NSArray<NSScreen *> *screens = [NSScreen screens];
    uint32_t count = (uint32_t)[screens count];
    
    ctx_base->monitor_cache = maru_context_alloc(ctx_base, sizeof(MARU_Monitor *) * count);
    ctx_base->monitor_cache_count = count;

    for (uint32_t i = 0; i < count; i++) {
        NSScreen *screen = screens[i];
        NSDictionary *deviceDescription = [screen deviceDescription];
        NSNumber *screenNumber = [deviceDescription objectForKey:@"NSScreenNumber"];
        CGDirectDisplayID displayID = (CGDirectDisplayID)[screenNumber unsignedIntValue];

        MARU_Monitor_Cocoa *mon = maru_context_alloc(ctx_base, sizeof(MARU_Monitor_Cocoa));
        memset(mon, 0, sizeof(MARU_Monitor_Cocoa));
        
        mon->base.ctx_base = ctx_base;
        mon->base.is_active = true;
        atomic_init(&mon->base.ref_count, 1u);
        
#ifdef MARU_INDIRECT_BACKEND
        mon->base.backend = &maru_backend_Cocoa;
#endif

        mon->display_id = displayID;
        mon->base.pub.is_primary = (i == 0);

        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
        if (mode) {
            mon->base.pub.current_mode.size.x = (int32_t)CGDisplayModeGetPixelWidth(mode);
            mon->base.pub.current_mode.size.y = (int32_t)CGDisplayModeGetPixelHeight(mode);
            mon->base.pub.current_mode.refresh_rate_millihz = (uint32_t)(CGDisplayModeGetRefreshRate(mode) * 1000.0);
            CGDisplayModeRelease(mode);
        } else {
            mon->base.pub.current_mode.size.x = (int32_t)CGDisplayPixelsWide(displayID);
            mon->base.pub.current_mode.size.y = (int32_t)CGDisplayPixelsHigh(displayID);
            mon->base.pub.current_mode.refresh_rate_millihz = 60000;
        }

        NSRect frame = [screen frame];
        mon->base.pub.logical_position.x = (MARU_Scalar)frame.origin.x;
        mon->base.pub.logical_position.y = (MARU_Scalar)frame.origin.y; 
        mon->base.pub.logical_size.x = (MARU_Scalar)frame.size.width;
        mon->base.pub.logical_size.y = (MARU_Scalar)frame.size.height;
        mon->base.pub.scale = [screen backingScaleFactor];

        NSSize displaySize = [[deviceDescription objectForKey:NSDeviceSize] sizeValue];
        mon->base.pub.physical_size.x = (MARU_Scalar)displaySize.width;
        mon->base.pub.physical_size.y = (MARU_Scalar)displaySize.height;

        ctx_base->monitor_cache[i] = (MARU_Monitor *)mon;
    }

    out_list->monitors = ctx_base->monitor_cache;
    out_list->count = count;
    return MARU_SUCCESS;
}

void maru_retainMonitor_Cocoa(MARU_Monitor *monitor) {
    MARU_Monitor_Base *base = (MARU_Monitor_Base *)monitor;
    atomic_fetch_add_explicit(&base->ref_count, 1u, memory_order_relaxed);
}

void maru_releaseMonitor_Cocoa(MARU_Monitor *monitor) {
    MARU_Monitor_Cocoa *mon = (MARU_Monitor_Cocoa *)monitor;
    MARU_Monitor_Base *base = (MARU_Monitor_Base *)monitor;
    uint32_t current = atomic_load_explicit(&base->ref_count, memory_order_acquire);
    while (current > 0u) {
        if (atomic_compare_exchange_weak_explicit(&base->ref_count,
                                                  &current,
                                                  current - 1u,
                                                  memory_order_acq_rel,
                                                  memory_order_acquire)) {
            if (current == 1u && !base->is_active) {
                if (mon->modes) {
                    maru_context_free(mon->base.ctx_base, mon->modes);
                    mon->modes = NULL;
                    mon->mode_count = 0;
                }
                maru_context_free(mon->base.ctx_base, mon);
            }
            return;
        }
    }
}

MARU_Status maru_getMonitorModes_Cocoa(const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
    MARU_Monitor_Cocoa *mon = (MARU_Monitor_Cocoa *)monitor;
    _maru_cocoa_populate_video_modes(mon);
    out_list->count = mon->mode_count;
    out_list->modes = mon->modes;
    return MARU_SUCCESS;
}

MARU_Status maru_setMonitorMode_Cocoa(const MARU_Monitor *monitor, MARU_VideoMode mode) {
    // Deprecated in modern macOS. Changing display resolution globally is frowned upon.
    // Applications should use Borderless Fullscreen Windows instead.
    // Implementing this would require CGDisplaySetDisplayMode, which causes flickering and re-layout.
    return MARU_FAILURE; 
}

MARU_Status maru_resetMonitorMetrics_Cocoa(MARU_Monitor *monitor) {
    MARU_Monitor_Cocoa *mon = (MARU_Monitor_Cocoa *)monitor;
    memset(&mon->base.metrics, 0, sizeof(MARU_MonitorMetrics));
    return MARU_SUCCESS;
}
