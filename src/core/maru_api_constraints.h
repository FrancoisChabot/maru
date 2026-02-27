// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_API_CONSTRAINTS_H_INCLUDED
#define MARU_API_CONSTRAINTS_H_INCLUDED

#include "maru_internal.h"
#include <stdlib.h>

#ifdef MARU_VALIDATE_API_CALLS

#define MARU_CONSTRAINT_CHECK(cond) \
    do { if (!(cond)) abort(); } while(0)

#include "maru/c/instrumentation.h"
#include "maru/c/vulkan.h"
#ifdef MARU_ENABLE_DIAGNOSTICS
extern void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg);
#endif
extern MARU_ThreadId _maru_getCurrentThreadId(void);

static inline void _maru_validate_thread(const MARU_Context_Base *ctx_base) {
    MARU_CONSTRAINT_CHECK(ctx_base->creator_thread == _maru_getCurrentThreadId());
}

static inline void _maru_validate_window_ready(const MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(maru_isWindowReady(window));
}

static inline void _maru_validate_window_not_lost(const MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(!maru_isWindowLost(window));
}

static inline void _maru_validate_monitor_not_lost(const MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(!maru_isMonitorLost(monitor));
}

static inline void _maru_validate_controller_not_lost(const MARU_Controller *controller) {
    MARU_CONSTRAINT_CHECK(!maru_isControllerLost(controller));
}

static inline bool _maru_validate_non_negative_vec2(MARU_Vec2Dip v) {
    return v.x >= 0 && v.y >= 0;
}

static inline bool _maru_validate_positive_vec2px(MARU_Vec2Px v) {
    return v.x > 0 && v.y > 0;
}

static inline bool _maru_validate_non_negative_rect(MARU_RectDip r) {
    return r.size.x >= 0 && r.size.y >= 0;
}

static inline bool _maru_validate_aspect_ratio(MARU_Fraction f) {
    if (f.num < 0 || f.denom < 0) {
        return false;
    }
    if (f.num == 0 || f.denom == 0) {
        return (f.num == 0 && f.denom == 0);
    }
    return true;
}

static inline void _maru_validate_attributes(const MARU_Context_Base *ctx_base, uint64_t field_mask,
                                             const MARU_WindowAttributes *attributes) {
    if (field_mask & MARU_WINDOW_ATTR_CURSOR_MODE) {
        MARU_CONSTRAINT_CHECK(attributes->cursor_mode >= MARU_CURSOR_NORMAL &&
                              attributes->cursor_mode <= MARU_CURSOR_LOCKED);
    }

    if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_TYPE) {
        MARU_CONSTRAINT_CHECK(attributes->text_input_type >= MARU_TEXT_INPUT_TYPE_NONE &&
                              attributes->text_input_type <= MARU_TEXT_INPUT_TYPE_NUMERIC);
    }

    if (field_mask & MARU_WINDOW_ATTR_ASPECT_RATIO) {
        MARU_CONSTRAINT_CHECK(_maru_validate_aspect_ratio(attributes->aspect_ratio));
    }

    if (field_mask & MARU_WINDOW_ATTR_LOGICAL_SIZE) {
        MARU_CONSTRAINT_CHECK(_maru_validate_non_negative_vec2(attributes->logical_size));
    }

    if (field_mask & MARU_WINDOW_ATTR_MIN_SIZE) {
        MARU_CONSTRAINT_CHECK(_maru_validate_non_negative_vec2(attributes->min_size));
    }

    if (field_mask & MARU_WINDOW_ATTR_MAX_SIZE) {
        MARU_CONSTRAINT_CHECK(_maru_validate_non_negative_vec2(attributes->max_size));
    }

    if ((field_mask & (MARU_WINDOW_ATTR_MIN_SIZE | MARU_WINDOW_ATTR_MAX_SIZE)) ==
        (MARU_WINDOW_ATTR_MIN_SIZE | MARU_WINDOW_ATTR_MAX_SIZE)) {
        const bool max_unbounded_x = attributes->max_size.x == 0;
        const bool max_unbounded_y = attributes->max_size.y == 0;
        if (!max_unbounded_x) {
            MARU_CONSTRAINT_CHECK(attributes->max_size.x >= attributes->min_size.x);
        }
        if (!max_unbounded_y) {
            MARU_CONSTRAINT_CHECK(attributes->max_size.y >= attributes->min_size.y);
        }
    }

    if (field_mask & MARU_WINDOW_ATTR_TEXT_INPUT_RECT) {
        MARU_CONSTRAINT_CHECK(_maru_validate_non_negative_rect(attributes->text_input_rect));
    }

    if (field_mask & MARU_WINDOW_ATTR_VIEWPORT_SIZE) {
        MARU_CONSTRAINT_CHECK(_maru_validate_non_negative_vec2(attributes->viewport_size));
    }

    if (field_mask & MARU_WINDOW_ATTR_MONITOR) {
        if (attributes->monitor != NULL) {
            MARU_CONSTRAINT_CHECK(((const MARU_Monitor_Base *)attributes->monitor)->ctx_base == ctx_base);
            _maru_validate_monitor_not_lost((const MARU_Monitor *)attributes->monitor);
        }
    }

    if (field_mask & MARU_WINDOW_ATTR_CURSOR) {
        if (attributes->cursor != NULL) {
            MARU_CONSTRAINT_CHECK(((const MARU_Cursor_Base *)attributes->cursor)->ctx_base == ctx_base);
        }
    }

    if (field_mask & MARU_WINDOW_ATTR_ICON) {
        if (attributes->icon != NULL) {
            MARU_CONSTRAINT_CHECK(((const MARU_Image_Base *)attributes->icon)->ctx_base == ctx_base);
        }
    }
}

static inline void _maru_validate_createContext(const MARU_ContextCreateInfo *create_info,
                                                       MARU_Context **out_context) {
    MARU_CONSTRAINT_CHECK(create_info != NULL);
    MARU_CONSTRAINT_CHECK(out_context != NULL);
}

static inline void _maru_validate_destroyContext(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void _maru_validate_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(attributes != NULL);
    _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void _maru_validate_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(callback != NULL);
    _maru_validate_thread((const MARU_Context_Base *)context);
    (void)timeout_ms;
    (void)userdata;
}

static inline void _maru_validate_createWindow(MARU_Context *context,
                                                     const MARU_WindowCreateInfo *create_info,
                                                     MARU_Window **out_window) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(create_info != NULL);
    MARU_CONSTRAINT_CHECK(out_window != NULL);
    
    const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
    _maru_validate_thread(ctx_base);
    _maru_validate_attributes(ctx_base, MARU_WINDOW_ATTR_ALL, &create_info->attributes);
}

static inline void _maru_validate_destroyWindow(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void _maru_validate_getWindowGeometry(MARU_Window *window,
                                              MARU_WindowGeometry *out_geometry) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(out_geometry != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_updateWindow(MARU_Window *window, uint64_t field_mask,
                                         const MARU_WindowAttributes *attributes) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(attributes != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);

    const MARU_Window_Base *win_base = (const MARU_Window_Base *)window;
    _maru_validate_thread(win_base->ctx_base);

    const uint64_t known_fields =
        MARU_WINDOW_ATTR_TITLE |
        MARU_WINDOW_ATTR_LOGICAL_SIZE |
        MARU_WINDOW_ATTR_FULLSCREEN |
        MARU_WINDOW_ATTR_CURSOR_MODE |
        MARU_WINDOW_ATTR_CURSOR |
        MARU_WINDOW_ATTR_MONITOR |
        MARU_WINDOW_ATTR_MAXIMIZED |
        MARU_WINDOW_ATTR_MIN_SIZE |
        MARU_WINDOW_ATTR_MAX_SIZE |
        MARU_WINDOW_ATTR_POSITION |
        MARU_WINDOW_ATTR_ASPECT_RATIO |
        MARU_WINDOW_ATTR_RESIZABLE |
        MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH |
        MARU_WINDOW_ATTR_ACCEPT_DROP |
        MARU_WINDOW_ATTR_TEXT_INPUT_TYPE |
        MARU_WINDOW_ATTR_TEXT_INPUT_RECT |
        MARU_WINDOW_ATTR_PRIMARY_SELECTION |
        MARU_WINDOW_ATTR_EVENT_MASK |
        MARU_WINDOW_ATTR_VIEWPORT_SIZE |
        MARU_WINDOW_ATTR_SURROUNDING_TEXT |
        MARU_WINDOW_ATTR_SURROUNDING_CURSOR_OFFSET |
        MARU_WINDOW_ATTR_VISIBLE |
        MARU_WINDOW_ATTR_MINIMIZED |
        MARU_WINDOW_ATTR_ICON;

    MARU_CONSTRAINT_CHECK((field_mask & ~known_fields) == 0);
    _maru_validate_attributes(win_base->ctx_base, field_mask, attributes);
}

static inline void _maru_validate_requestWindowFocus(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
    _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void _maru_validate_requestWindowFrame(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
    _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void _maru_validate_requestWindowAttention(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
    _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void _maru_validate_resetContextMetrics(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void _maru_validate_resetWindowMetrics(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    _maru_validate_window_not_lost(window);
    _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
}

static inline void _maru_validate_createCursor(MARU_Context *context,
                                                     const MARU_CursorCreateInfo *create_info,
                                                     MARU_Cursor **out_cursor) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(create_info != NULL);
    MARU_CONSTRAINT_CHECK(out_cursor != NULL);
    
    const MARU_Context_Base *ctx_base = (const MARU_Context_Base *)context;
    _maru_validate_thread(ctx_base);

    MARU_CONSTRAINT_CHECK(create_info->source == MARU_CURSOR_SOURCE_CUSTOM ||
                          create_info->source == MARU_CURSOR_SOURCE_SYSTEM);
    
    if (create_info->source == MARU_CURSOR_SOURCE_CUSTOM) {
        MARU_CONSTRAINT_CHECK(create_info->frame_count > 0);
        MARU_CONSTRAINT_CHECK(create_info->frames != NULL);
        for (uint32_t i = 0; i < create_info->frame_count; ++i) {
            MARU_CONSTRAINT_CHECK(create_info->frames[i].image != NULL);
            MARU_CONSTRAINT_CHECK(((const MARU_Image_Base *)create_info->frames[i].image)->ctx_base == ctx_base);
        }
    } else {
        MARU_CONSTRAINT_CHECK(create_info->system_shape >= MARU_CURSOR_SHAPE_DEFAULT &&
                              create_info->system_shape <= MARU_CURSOR_SHAPE_NWSE_RESIZE);
    }
}

static inline void _maru_validate_destroyCursor(MARU_Cursor *cursor) {
    MARU_CONSTRAINT_CHECK(cursor != NULL);
    _maru_validate_thread(((const MARU_Cursor_Base *)cursor)->ctx_base);
}

static inline void _maru_validate_resetCursorMetrics(MARU_Cursor *cursor) {
    MARU_CONSTRAINT_CHECK(cursor != NULL);
    _maru_validate_thread(((const MARU_Cursor_Base *)cursor)->ctx_base);
}

static inline void _maru_validate_createImage(MARU_Context *context,
                                              const MARU_ImageCreateInfo *create_info,
                                              MARU_Image **out_image) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(create_info != NULL);
    MARU_CONSTRAINT_CHECK(out_image != NULL);
    _maru_validate_thread((const MARU_Context_Base *)context);

    MARU_CONSTRAINT_CHECK(_maru_validate_positive_vec2px(create_info->size));
    MARU_CONSTRAINT_CHECK(create_info->pixels != NULL);
    if (create_info->stride_bytes != 0) {
        MARU_CONSTRAINT_CHECK(create_info->stride_bytes >= (uint32_t)create_info->size.x * 4u);
    }
}

static inline void _maru_validate_destroyImage(MARU_Image *image) {
    MARU_CONSTRAINT_CHECK(image != NULL);
    _maru_validate_thread(((const MARU_Image_Base *)image)->ctx_base);
}

static inline void _maru_validate_getControllers(MARU_Context *context,
                                                 MARU_ControllerList *out_list) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_list != NULL);
    _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void _maru_validate_retainController(MARU_Controller *controller) {
    MARU_CONSTRAINT_CHECK(controller != NULL);
}

static inline void _maru_validate_releaseController(MARU_Controller *controller) {
    MARU_CONSTRAINT_CHECK(controller != NULL);
}

static inline void _maru_validate_resetControllerMetrics(MARU_Controller *controller) {
    MARU_CONSTRAINT_CHECK(controller != NULL);
    _maru_validate_thread((const MARU_Context_Base *)maru_getControllerContext(controller));
    _maru_validate_controller_not_lost(controller);
}

static inline void _maru_validate_getControllerInfo(MARU_Controller *controller,
                                                    MARU_ControllerInfo *out_info) {
    MARU_CONSTRAINT_CHECK(controller != NULL);
    MARU_CONSTRAINT_CHECK(out_info != NULL);
    _maru_validate_thread((const MARU_Context_Base *)maru_getControllerContext(controller));
    _maru_validate_controller_not_lost(controller);
}

static inline void _maru_validate_setControllerHapticLevels(
    MARU_Controller *controller, uint32_t first_haptic, uint32_t count,
    const MARU_Scalar *intensities) {
    MARU_CONSTRAINT_CHECK(controller != NULL);
    _maru_validate_thread((const MARU_Context_Base *)maru_getControllerContext(controller));
    _maru_validate_controller_not_lost(controller);
    (void)first_haptic;
    if (count > 0) {
        MARU_CONSTRAINT_CHECK(intensities != NULL);
    }
}

static inline void _maru_validate_announceData(MARU_Window *window,
                                               MARU_DataExchangeTarget target,
                                               const char **mime_types,
                                               uint32_t count,
                                               MARU_DropActionMask allowed_actions) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
    _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
    MARU_CONSTRAINT_CHECK(target >= MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
                          target <= MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
    if (count > 0) {
        MARU_CONSTRAINT_CHECK(mime_types != NULL);
    }
    (void)allowed_actions;
}

static inline void _maru_validate_provideData(
    const MARU_DataRequestEvent *request_event, const void *data, size_t size,
    MARU_DataProvideFlags flags) {
    MARU_CONSTRAINT_CHECK(request_event != NULL);
    MARU_CONSTRAINT_CHECK(request_event->internal_handle != NULL);
    const MARU_DataRequestHandleBase *handle = (const MARU_DataRequestHandleBase *)request_event->internal_handle;
    _maru_validate_thread(handle->ctx_base);
    if (size > 0) {
        MARU_CONSTRAINT_CHECK(data != NULL);
    }
    (void)flags;
}

static inline void _maru_validate_requestData(MARU_Window *window,
                                              MARU_DataExchangeTarget target,
                                              const char *mime_type,
                                              void *user_tag) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
    _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
    MARU_CONSTRAINT_CHECK(target >= MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
                          target <= MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
    MARU_CONSTRAINT_CHECK(mime_type != NULL);
    (void)user_tag;
}

static inline void _maru_validate_getAvailableMIMETypes(
    MARU_Window *window, MARU_DataExchangeTarget target,
    MARU_MIMETypeList *out_list) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
    _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
    MARU_CONSTRAINT_CHECK(target >= MARU_DATA_EXCHANGE_TARGET_CLIPBOARD &&
                          target <= MARU_DATA_EXCHANGE_TARGET_DRAG_DROP);
    MARU_CONSTRAINT_CHECK(out_list != NULL);
}

static inline void _maru_validate_wakeContext(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL);
}

static inline void _maru_validate_postEvent(MARU_Context *context, MARU_EventId type,
                                            MARU_Window *window, MARU_UserDefinedEvent evt) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(maru_isUserEventId(type));
    if (window != NULL) {
        MARU_CONSTRAINT_CHECK(maru_getWindowContext(window) == context);
    }
    (void)type;
    (void)evt;
}

static inline void _maru_validate_getMonitors(MARU_Context *context, uint32_t *out_count) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_count != NULL);
    _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void _maru_validate_retainMonitor(MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
}

static inline void _maru_validate_releaseMonitor(MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
}

static inline void _maru_validate_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
    _maru_validate_monitor_not_lost(monitor);
    MARU_CONSTRAINT_CHECK(out_count != NULL);
}

static inline void _maru_validate_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
    _maru_validate_monitor_not_lost(monitor);
    _maru_validate_thread(((const MARU_Monitor_Base *)monitor)->ctx_base);
    MARU_CONSTRAINT_CHECK(mode.size.x > 0 && mode.size.y > 0);
}

static inline void _maru_validate_resetMonitorMetrics(MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
    _maru_validate_monitor_not_lost(monitor);
    _maru_validate_thread(((const MARU_Monitor_Base *)monitor)->ctx_base);
}

static inline void _maru_validate_getWaylandContextHandle(MARU_Context *context, void *out_handle) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
}

static inline void _maru_validate_getWaylandWindowHandle(MARU_Window *window, void *out_handle) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_getX11ContextHandle(MARU_Context *context, void *out_handle) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
}

static inline void _maru_validate_getX11WindowHandle(MARU_Window *window, void *out_handle) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_getWin32ContextHandle(MARU_Context *context, void *out_handle) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
}

static inline void _maru_validate_getWin32WindowHandle(MARU_Window *window, void *out_handle) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_getCocoaContextHandle(MARU_Context *context, void *out_handle) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
}

static inline void _maru_validate_getCocoaWindowHandle(MARU_Window *window, void *out_handle) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_getLinuxContextHandle(MARU_Context *context, void *out_handle) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
}

static inline void _maru_validate_getLinuxWindowHandle(MARU_Window *window, void *out_handle) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
}

static inline void _maru_validate_getVkExtensions(const MARU_Context *context, uint32_t *out_count) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_count != NULL);
    _maru_validate_thread((const MARU_Context_Base *)context);
}

static inline void _maru_validate_createVkSurface(MARU_Window *window, 
                                 VkInstance instance,
                                 MARU_VkGetInstanceProcAddrFunc vk_loader,
                                 VkSurfaceKHR *out_surface) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    _maru_validate_window_ready(window);
    _maru_validate_window_not_lost(window);
    MARU_CONSTRAINT_CHECK(instance != NULL);
    MARU_CONSTRAINT_CHECK(out_surface != NULL);
    _maru_validate_thread(((const MARU_Window_Base *)window)->ctx_base);
    (void)vk_loader;
}

#define MARU_API_VALIDATE(fn, ...) \
    _maru_validate_##fn(__VA_ARGS__)

#else
#define MARU_API_VALIDATE(fn, ...) (void)0
#endif

#endif // MARU_API_CONSTRAINTS_H_INCLUDED
