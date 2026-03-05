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
        vm->refresh_rate_mhz = (uint32_t)(CGDisplayModeGetRefreshRate(mode) * 1000000.0);
        if (vm->refresh_rate_mhz == 0) vm->refresh_rate_mhz = 60000000;
    }
    CFRelease(displayModes);
}

MARU_Monitor *const *maru_getMonitors_Cocoa(MARU_Context *context, uint32_t *out_count) {
    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;

    if (ctx_base->monitor_cache) {
        if (out_count) *out_count = ctx_base->monitor_cache_count;
        return ctx_base->monitor_cache;
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
        
#ifdef MARU_INDIRECT_BACKEND
        mon->base.backend = &maru_backend_Cocoa;
#endif

        mon->display_id = displayID;
        mon->base.pub.is_primary = (i == 0);

        CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
        if (mode) {
            mon->base.pub.current_mode.size.x = (int32_t)CGDisplayModeGetPixelWidth(mode);
            mon->base.pub.current_mode.size.y = (int32_t)CGDisplayModeGetPixelHeight(mode);
            mon->base.pub.current_mode.refresh_rate_mhz = (uint32_t)(CGDisplayModeGetRefreshRate(mode) * 1000000.0);
            CGDisplayModeRelease(mode);
        } else {
            mon->base.pub.current_mode.size.x = (int32_t)CGDisplayPixelsWide(displayID);
            mon->base.pub.current_mode.size.y = (int32_t)CGDisplayPixelsHigh(displayID);
            mon->base.pub.current_mode.refresh_rate_mhz = 60000000;
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

    if (out_count) *out_count = count;
    return ctx_base->monitor_cache;
}

void maru_retainMonitor_Cocoa(MARU_Monitor *monitor) {
    MARU_Monitor_Cocoa *mon = (MARU_Monitor_Cocoa *)monitor;
    // Base ref counting is handled by maru_retainMonitor in core.c, 
    // but the backend can intercept. We just rely on core here or implement custom logic if needed.
    (void)mon;
}

void maru_releaseMonitor_Cocoa(MARU_Monitor *monitor) {
    MARU_Monitor_Cocoa *mon = (MARU_Monitor_Cocoa *)monitor;
    if (mon->modes) {
        maru_context_free(mon->base.ctx_base, mon->modes);
    }
    maru_context_free(mon->base.ctx_base, mon);
}

const MARU_VideoMode *maru_getMonitorModes_Cocoa(const MARU_Monitor *monitor, uint32_t *out_count) {
    MARU_Monitor_Cocoa *mon = (MARU_Monitor_Cocoa *)monitor;
    _maru_cocoa_populate_video_modes(mon);
    if (out_count) *out_count = mon->mode_count;
    return mon->modes;
}

MARU_Status maru_setMonitorMode_Cocoa(const MARU_Monitor *monitor, MARU_VideoMode mode) {
    // Deprecated in modern macOS. Changing display resolution globally is frowned upon.
    // Applications should use Borderless Fullscreen Windows instead.
    // Implementing this would require CGDisplaySetDisplayMode, which causes flickering and re-layout.
    return MARU_FAILURE; 
}

void maru_resetMonitorMetrics_Cocoa(MARU_Monitor *monitor) {
    MARU_Monitor_Cocoa *mon = (MARU_Monitor_Cocoa *)monitor;
    memset(&mon->base.metrics, 0, sizeof(MARU_MonitorMetrics));
}
