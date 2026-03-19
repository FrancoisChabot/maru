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
        vm->px_size.x = (int32_t)CGDisplayModeGetPixelWidth(mode);
        vm->px_size.y = (int32_t)CGDisplayModeGetPixelHeight(mode);
        vm->refresh_rate_millihz = (uint32_t)(CGDisplayModeGetRefreshRate(mode) * 1000.0);
        if (vm->refresh_rate_millihz == 0) vm->refresh_rate_millihz = 60000;
    }
    CFRelease(displayModes);
}

static void _maru_cocoa_update_monitor_from_screen(MARU_Monitor_Cocoa *mon, NSScreen *screen, bool is_primary) {
    NSDictionary *deviceDescription = [screen deviceDescription];
    NSNumber *screenNumber = [deviceDescription objectForKey:@"NSScreenNumber"];
    CGDirectDisplayID displayID = (CGDirectDisplayID)[screenNumber unsignedIntValue];
    mon->display_id = displayID;

    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(displayID);
    if (mode) {
        mon->base.pub.current_mode.px_size.x = (int32_t)CGDisplayModeGetPixelWidth(mode);
        mon->base.pub.current_mode.px_size.y = (int32_t)CGDisplayModeGetPixelHeight(mode);
        mon->base.pub.current_mode.refresh_rate_millihz = (uint32_t)(CGDisplayModeGetRefreshRate(mode) * 1000.0);
        CGDisplayModeRelease(mode);
    } else {
        mon->base.pub.current_mode.px_size.x = (int32_t)CGDisplayPixelsWide(displayID);
        mon->base.pub.current_mode.px_size.y = (int32_t)CGDisplayPixelsHigh(displayID);
        mon->base.pub.current_mode.refresh_rate_millihz = 60000;
    }

    NSRect frame = [screen frame];
    mon->base.pub.dip_position.x = (MARU_Scalar)frame.origin.x;
    mon->base.pub.dip_position.y = (MARU_Scalar)frame.origin.y; 
    mon->base.pub.dip_size.x = (MARU_Scalar)frame.size.width;
    mon->base.pub.dip_size.y = (MARU_Scalar)frame.size.height;
    mon->base.pub.scale = (MARU_Scalar)[screen backingScaleFactor];
    mon->base.pub.is_primary = is_primary;

    NSSize displaySize = [[deviceDescription objectForKey:NSDeviceSize] sizeValue];
    mon->base.pub.physical_size.x = (MARU_Scalar)displaySize.width;
    mon->base.pub.physical_size.y = (MARU_Scalar)displaySize.height;
    
    // Invalidate modes if needed.
    if (mon->modes) {
        maru_context_free(mon->base.ctx_base, mon->modes);
        mon->modes = NULL;
        mon->mode_count = 0;
    }
}

void _maru_cocoa_refresh_monitors(MARU_Context_Base *ctx_base) {
    NSArray<NSScreen *> *screens = [NSScreen screens];
    uint32_t new_count = (uint32_t)[screens count];
    
    MARU_Monitor **new_cache = maru_context_alloc(ctx_base, sizeof(MARU_Monitor *) * new_count);
    
    // 1. Mark all old monitors as potentially removed
    for (uint32_t i = 0; i < ctx_base->monitor_cache_count; ++i) {
        MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)ctx_base->monitor_cache[i];
        mon_base->is_active = false;
    }

    for (uint32_t i = 0; i < new_count; i++) {
        NSScreen *screen = screens[i];
        NSDictionary *deviceDescription = [screen deviceDescription];
        NSNumber *screenNumber = [deviceDescription objectForKey:@"NSScreenNumber"];
        CGDirectDisplayID displayID = (CGDirectDisplayID)[screenNumber unsignedIntValue];

        // 2. See if we already have this monitor
        MARU_Monitor_Cocoa *mon = NULL;
        for (uint32_t j = 0; j < ctx_base->monitor_cache_count; j++) {
            MARU_Monitor_Cocoa *old_mon = (MARU_Monitor_Cocoa *)ctx_base->monitor_cache[j];
            if (old_mon->display_id == displayID) {
                mon = old_mon;
                break;
            }
        }

        bool is_new = false;
        if (!mon) {
            mon = maru_context_alloc(ctx_base, sizeof(MARU_Monitor_Cocoa));
            memset(mon, 0, sizeof(MARU_Monitor_Cocoa));
            mon->base.ctx_base = ctx_base;
            mon->base.pub.context = (MARU_Context *)ctx_base;
            atomic_init(&mon->base.ref_count, 1u);
#ifdef MARU_INDIRECT_BACKEND
            mon->base.backend = &maru_backend_Cocoa;
#endif
            is_new = true;
        }

        mon->base.is_active = true;
        
        MARU_VideoMode old_mode = mon->base.pub.current_mode;
        MARU_Scalar old_scale = mon->base.pub.scale;

        _maru_cocoa_update_monitor_from_screen(mon, screen, (i == 0));
        new_cache[i] = (MARU_Monitor *)mon;

        if (is_new) {
            MARU_Event evt = {0};
            evt.monitor_changed.monitor = (MARU_Monitor *)mon;
            evt.monitor_changed.connected = true;
            _maru_post_event_internal(ctx_base, MARU_EVENT_MONITOR_CHANGED, NULL, &evt);
        } else {
            if (old_mode.px_size.x != mon->base.pub.current_mode.px_size.x ||
                old_mode.px_size.y != mon->base.pub.current_mode.px_size.y ||
                old_mode.refresh_rate_millihz != mon->base.pub.current_mode.refresh_rate_millihz ||
                old_scale != mon->base.pub.scale) {
                
                MARU_Event evt = {0};
                evt.monitor_mode_changed.monitor = (MARU_Monitor *)mon;
                _maru_post_event_internal(ctx_base, MARU_EVENT_MONITOR_MODE_CHANGED, NULL, &evt);
            }
        }
    }

    // 3. Emit disconnected events for monitors that are no longer active
    for (uint32_t i = 0; i < ctx_base->monitor_cache_count; i++) {
        MARU_Monitor_Base *mon_base = (MARU_Monitor_Base *)ctx_base->monitor_cache[i];
        if (!mon_base->is_active) {
            MARU_Event evt = {0};
            evt.monitor_changed.monitor = (MARU_Monitor *)mon_base;
            evt.monitor_changed.connected = false;
            _maru_post_event_internal(ctx_base, MARU_EVENT_MONITOR_CHANGED, NULL, &evt);
            
            // If ref_count is 1 (only our cache has it), it will be freed when cache is cleared.
            // Wait, ref_count in cache is what we maintain.
            // Actually, we should release it if it's no longer in cache.
            maru_releaseMonitor_Cocoa((MARU_Monitor *)mon_base);
        }
    }

    maru_context_free(ctx_base, ctx_base->monitor_cache);
    ctx_base->monitor_cache = new_cache;
    ctx_base->monitor_cache_count = new_count;
}

MARU_Status maru_getMonitors_Cocoa(const MARU_Context *context, MARU_MonitorList *out_list) {
    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;

    if (!ctx_base->monitor_cache) {
        _maru_cocoa_refresh_monitors(ctx_base);
    }

    out_list->monitors = ctx_base->monitor_cache;
    out_list->count = ctx_base->monitor_cache_count;
    return MARU_SUCCESS;
}

void maru_retainMonitor_Cocoa(MARU_Monitor *monitor) {
    MARU_Monitor_Base *base = (MARU_Monitor_Base *)monitor;
    atomic_fetch_add_explicit(&base->ref_count, 1u, memory_order_relaxed);
}

void _maru_monitor_free(MARU_Monitor_Base *monitor) {
    MARU_Monitor_Cocoa *mon = (MARU_Monitor_Cocoa *)monitor;
    if (mon->modes) {
        maru_context_free(mon->base.ctx_base, mon->modes);
        mon->modes = NULL;
        mon->mode_count = 0;
    }
    maru_context_free(mon->base.ctx_base, mon);
}

void maru_releaseMonitor_Cocoa(MARU_Monitor *monitor) {
    MARU_Monitor_Base *base = (MARU_Monitor_Base *)monitor;
    uint32_t current = atomic_load_explicit(&base->ref_count, memory_order_acquire);
    while (current > 0u) {
        if (atomic_compare_exchange_weak_explicit(&base->ref_count,
                                                  &current,
                                                  current - 1u,
                                                  memory_order_acq_rel,
                                                  memory_order_acquire)) {
            if (current == 1u && !base->is_active) {
                _maru_monitor_free(base);
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

MARU_Status maru_setMonitorMode_Cocoa(MARU_Monitor *monitor, MARU_VideoMode mode) {
    MARU_Monitor_Cocoa *mon = (MARU_Monitor_Cocoa *)monitor;
    
    CFArrayRef displayModes = CGDisplayCopyAllDisplayModes(mon->display_id, NULL);
    if (!displayModes) return MARU_FAILURE;

    CGDisplayModeRef bestMode = NULL;
    CFIndex count = CFArrayGetCount(displayModes);
    for (CFIndex i = 0; i < count; i++) {
        CGDisplayModeRef m = (CGDisplayModeRef)CFArrayGetValueAtIndex(displayModes, i);
        int32_t width = (int32_t)CGDisplayModeGetPixelWidth(m);
        int32_t height = (int32_t)CGDisplayModeGetPixelHeight(m);
        uint32_t refresh = (uint32_t)(CGDisplayModeGetRefreshRate(m) * 1000.0);
        if (refresh == 0) refresh = 60000;

        if (width == mode.px_size.x && height == mode.px_size.y && refresh == mode.refresh_rate_millihz) {
            bestMode = m;
            break;
        }
    }

    MARU_Status status = MARU_FAILURE;
    if (bestMode) {
        if (CGDisplaySetDisplayMode(mon->display_id, bestMode, NULL) == kCGErrorSuccess) {
            status = MARU_SUCCESS;
            
            // Update current mode in public struct
            mon->base.pub.current_mode.px_size.x = (int32_t)CGDisplayModeGetPixelWidth(bestMode);
            mon->base.pub.current_mode.px_size.y = (int32_t)CGDisplayModeGetPixelHeight(bestMode);
            mon->base.pub.current_mode.refresh_rate_millihz = (uint32_t)(CGDisplayModeGetRefreshRate(bestMode) * 1000.0);
            if (mon->base.pub.current_mode.refresh_rate_millihz == 0) mon->base.pub.current_mode.refresh_rate_millihz = 60000;

            MARU_Event evt = {0};
            evt.monitor_mode_changed.monitor = (MARU_Monitor *)mon;
            _maru_post_event_internal(mon->base.ctx_base, MARU_EVENT_MONITOR_MODE_CHANGED, NULL, &evt);
        }
    }

    CFRelease(displayModes) ;
    return status;
}
