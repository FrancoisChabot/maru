#ifndef MARU_API_CONSTRAINTS_H_INCLUDED
#define MARU_API_CONSTRAINTS_H_INCLUDED

#include "maru_internal.h"
#include <stdlib.h>

#ifdef MARU_VALIDATE_API_CALLS

#define MARU_CONSTRAINT_CHECK(cond) \
    do { if (!(cond)) abort(); } while(0)

// We'll implement these in core.c or a new diagnostics.c
#include "maru/c/instrumentation.h"
#ifdef MARU_ENABLE_DIAGNOSTICS
extern void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg);
#endif
extern MARU_ThreadId _maru_getCurrentThreadId(void);

static inline bool _maru_validate_non_negative_vec2(MARU_Vec2Dip v) {
    return v.x >= 0 && v.y >= 0;
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

static inline void _maru_validate_createContext(const MARU_ContextCreateInfo *create_info,
                                                       MARU_Context **out_context) {
    MARU_CONSTRAINT_CHECK(create_info != NULL);
    MARU_CONSTRAINT_CHECK(out_context != NULL);
}

static inline void _maru_validate_destroyContext(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    
    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
    MARU_CONSTRAINT_CHECK(ctx_base->creator_thread == _maru_getCurrentThreadId());
}

static inline void _maru_validate_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(attributes != NULL);
    
    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
    MARU_CONSTRAINT_CHECK(ctx_base->creator_thread == _maru_getCurrentThreadId());
}

static inline void _maru_validate_pumpEvents(MARU_Context *context, uint32_t timeout_ms, MARU_EventCallback callback, void *userdata) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(callback != NULL);
    (void)timeout_ms;
    (void)userdata;
}

static inline void _maru_validate_createWindow(MARU_Context *context,
                                                     const MARU_WindowCreateInfo *create_info,
                                                     MARU_Window **out_window) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(create_info != NULL);
    MARU_CONSTRAINT_CHECK(out_window != NULL);
}

static inline void _maru_validate_destroyWindow(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
}

static inline void _maru_validate_getWindowGeometry(MARU_Window *window,
                                              MARU_WindowGeometry *out_geometry) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(out_geometry != NULL);
}

static inline void _maru_validate_updateWindow(MARU_Window *window, uint64_t field_mask,
                                         const MARU_WindowAttributes *attributes) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(attributes != NULL);

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
        MARU_WINDOW_ATTR_TEXT_INPUT_TYPE |
        MARU_WINDOW_ATTR_TEXT_INPUT_RECT |
        MARU_WINDOW_ATTR_EVENT_MASK |
        MARU_WINDOW_ATTR_VIEWPORT_SIZE |
        MARU_WINDOW_ATTR_SURROUNDING_TEXT |
        MARU_WINDOW_ATTR_SURROUNDING_CURSOR_OFFSET |
        MARU_WINDOW_ATTR_VISIBLE |
        MARU_WINDOW_ATTR_MINIMIZED |
        MARU_WINDOW_ATTR_ICON;

    MARU_CONSTRAINT_CHECK((field_mask & ~known_fields) == 0);

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
            MARU_CONSTRAINT_CHECK(maru_getMonitorContext(attributes->monitor) ==
                                  maru_getWindowContext(window));
        }
    }

    if (field_mask & MARU_WINDOW_ATTR_ICON) {
        if (attributes->icon != NULL) {
            const MARU_Image_Base *img = (const MARU_Image_Base *)attributes->icon;
            const MARU_Window_Base *win = (const MARU_Window_Base *)window;
            MARU_CONSTRAINT_CHECK(img->ctx_base == win->ctx_base);
        }
    }
}

static inline void _maru_validate_requestWindowFocus(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
}

static inline void _maru_validate_requestWindowFrame(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
}

static inline void _maru_validate_requestWindowAttention(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
}

static inline void _maru_validate_resetContextMetrics(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL);
}

static inline void _maru_validate_resetWindowMetrics(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
}

static inline void _maru_validate_createCursor(MARU_Context *context,
                                                     const MARU_CursorCreateInfo *create_info,
                                                     MARU_Cursor **out_cursor) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(create_info != NULL);
    MARU_CONSTRAINT_CHECK(out_cursor != NULL);
}

static inline void _maru_validate_destroyCursor(MARU_Cursor *cursor) {
    MARU_CONSTRAINT_CHECK(cursor != NULL);
}

static inline void _maru_validate_resetCursorMetrics(MARU_Cursor *cursor) {
    MARU_CONSTRAINT_CHECK(cursor != NULL);
}

static inline void _maru_validate_createImage(MARU_Context *context,
                                              const MARU_ImageCreateInfo *create_info,
                                              MARU_Image **out_image) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(create_info != NULL);
    MARU_CONSTRAINT_CHECK(out_image != NULL);
}

static inline void _maru_validate_destroyImage(MARU_Image *image) {
    MARU_CONSTRAINT_CHECK(image != NULL);
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
}

static inline void _maru_validate_retainMonitor(MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
}

static inline void _maru_validate_releaseMonitor(MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
}

static inline void _maru_validate_getMonitorModes(const MARU_Monitor *monitor, uint32_t *out_count) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
    MARU_CONSTRAINT_CHECK(out_count != NULL);
}

static inline void _maru_validate_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
    (void)mode;
}

static inline void _maru_validate_resetMonitorMetrics(MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(monitor != NULL);
}

#define MARU_API_VALIDATE(fn, ...) \
    _maru_validate_##fn(__VA_ARGS__)

#else
#define MARU_API_VALIDATE(fn, ...) (void)0
#endif

#endif // MARU_API_CONSTRAINTS_H_INCLUDED
