#ifndef MARU_API_CONSTRAINTS_H_INCLUDED
#define MARU_API_CONSTRAINTS_H_INCLUDED

#include "maru_internal.h"
#include <stdlib.h>

#ifdef MARU_VALIDATE_API_CALLS

#ifdef MARU_RECOVERABLE_API_CALLS
#define MARU_CONSTRAINT_CHECK(cond, status) \
    do { if (!(cond)) return (status); } while(0)
#else
#define MARU_CONSTRAINT_CHECK(cond, status) \
    do { if (!(cond)) abort(); } while(0)
#endif

// We'll implement these in core.c or a new diagnostics.c
#include "maru/c/instrumentation.h"
extern void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg);
extern MARU_ThreadId _maru_getCurrentThreadId(void);

static inline MARU_Status _maru_validate_createContext(const MARU_ContextCreateInfo *create_info,
                                                       MARU_Context **out_context) {
    MARU_CONSTRAINT_CHECK(create_info != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_context != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_destroyContext(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    
    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
    MARU_CONSTRAINT_CHECK(ctx_base->creator_thread == _maru_getCurrentThreadId(), MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_updateContext(MARU_Context *context, uint64_t field_mask,
                                          const MARU_ContextAttributes *attributes) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(attributes != NULL, MARU_ERROR_INVALID_USAGE);
    
    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
    MARU_CONSTRAINT_CHECK(ctx_base->creator_thread == _maru_getCurrentThreadId(), MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_pumpEvents(MARU_Context *context, uint32_t timeout_ms) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    (void)timeout_ms;
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_createWindow(MARU_Context *context,
                                                     const MARU_WindowCreateInfo *create_info,
                                                     MARU_Window **out_window) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(create_info != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_window != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_destroyWindow(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_getWindowGeometry(MARU_Window *window,
                                              MARU_WindowGeometry *out_geometry) {
    MARU_CONSTRAINT_CHECK(window != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_geometry != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_updateWindow(MARU_Window *window, uint64_t field_mask,
                                         const MARU_WindowAttributes *attributes) {
    MARU_CONSTRAINT_CHECK(window != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(attributes != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_requestWindowFocus(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_getWindowBackendHandle(MARU_Window *window,
                                               MARU_BackendType *out_type,
                                               MARU_BackendHandle *out_handle) {
    MARU_CONSTRAINT_CHECK(window != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_type != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_handle != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_resetContextMetrics(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_resetWindowMetrics(MARU_Window *window) {
    MARU_CONSTRAINT_CHECK(window != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_getStandardCursor(MARU_Context *context, MARU_CursorShape shape,
                                                            MARU_Cursor **out_cursor) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_cursor != NULL, MARU_ERROR_INVALID_USAGE);
    (void)shape;
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_createCursor(MARU_Context *context,
                                                     const MARU_CursorCreateInfo *create_info,
                                                     MARU_Cursor **out_cursor) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(create_info != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_cursor != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_destroyCursor(MARU_Cursor *cursor) {
    MARU_CONSTRAINT_CHECK(cursor != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_resetCursorMetrics(MARU_Cursor *cursor) {
    MARU_CONSTRAINT_CHECK(cursor != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_wakeContext(MARU_Context *context) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_getMonitors(MARU_Context *context, MARU_MonitorList *out_list) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_list != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_retainMonitor(MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(monitor != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_releaseMonitor(MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(monitor != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_getMonitorModes(const MARU_Monitor *monitor, MARU_VideoModeList *out_list) {
    MARU_CONSTRAINT_CHECK(monitor != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_list != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_setMonitorMode(const MARU_Monitor *monitor, MARU_VideoMode mode) {
    MARU_CONSTRAINT_CHECK(monitor != NULL, MARU_ERROR_INVALID_USAGE);
    (void)mode;
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_resetMonitorMetrics(MARU_Monitor *monitor) {
    MARU_CONSTRAINT_CHECK(monitor != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

#ifdef MARU_ENABLE_VULKAN
static inline MARU_Status _maru_validate_getVkExtensions(MARU_Context *context, MARU_ExtensionList *out_list) {
    MARU_CONSTRAINT_CHECK(context != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_list != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}

static inline MARU_Status _maru_validate_createVkSurface(MARU_Window *window, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface) {
    MARU_CONSTRAINT_CHECK(window != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(instance != NULL, MARU_ERROR_INVALID_USAGE);
    MARU_CONSTRAINT_CHECK(out_surface != NULL, MARU_ERROR_INVALID_USAGE);
    return MARU_SUCCESS;
}
#endif

#define MARU_API_VALIDATE(fn, ...) \
    do { \
        MARU_Status _res = _maru_validate_##fn(__VA_ARGS__); \
        if (_res != MARU_SUCCESS) return _res; \
    } while (0)

#else
#define MARU_API_VALIDATE(fn, ...) (void)0
#endif

#endif // MARU_API_CONSTRAINTS_H_INCLUDED
