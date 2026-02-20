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
}

static inline void _maru_validate_requestWindowFocus(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
}

static inline void _maru_validate_getWindowBackendHandle(MARU_Window *window,
                                               MARU_BackendType *out_type,
                                               MARU_BackendHandle *out_handle) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(out_type != NULL);
    MARU_CONSTRAINT_CHECK(out_handle != NULL);
}

static inline void _maru_validate_resetContextMetrics(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL);
}

static inline void _maru_validate_resetWindowMetrics(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL);
}

static inline void _maru_validate_getStandardCursor(MARU_Context *context, MARU_CursorShape shape,
                                                            MARU_Cursor **out_cursor) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_cursor != NULL);
    (void)shape;
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
    MARU_CONSTRAINT_CHECK(!maru_isCursorSystem(cursor));
}

static inline void _maru_validate_resetCursorMetrics(MARU_Cursor *cursor) {
    MARU_CONSTRAINT_CHECK(cursor != NULL);
}

static inline void _maru_validate_wakeContext(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL);
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
