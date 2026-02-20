#ifndef MARU_API_CONSTRAINTS_H_INCLUDED
#define MARU_API_CONSTRAINTS_H_INCLUDED

#include "maru_internal.h"
#include <stdlib.h>

#ifdef MARU_VALIDATE_API_CALLS

#define MARU_CONSTRAINT_CHECK(cond) \
    do { if (!(cond)) abort(); } while(0)

// We'll implement these in core.c or a new diagnostics.c
#include "maru/c/instrumentation.h"
extern void MARU_REPORT_DIAGNOSTIC(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg);
extern MARU_ThreadId _maru_getCurrentThreadId(void);

static inline void _maru_validate_getVkExtensions(MARU_Context *context, uint32_t *out_count) {
    MARU_CONSTRAINT_CHECK(context != NULL);
    MARU_CONSTRAINT_CHECK(out_count != NULL);
}

static inline void _maru_validate_createVkSurface(MARU_Window *window, 
                                 VkInstance instance,
                                 VkSurfaceKHR *out_surface) {
    MARU_CONSTRAINT_CHECK(window != NULL);
    MARU_CONSTRAINT_CHECK(instance != NULL);
    MARU_CONSTRAINT_CHECK(out_surface != NULL);
}

#define MARU_API_VALIDATE(fn, ...) \
    _maru_validate_##fn(__VA_ARGS__)

#else
#define MARU_API_VALIDATE(fn, ...) (void)0
#endif

#endif // MARU_API_CONSTRAINTS_H_INCLUDED
