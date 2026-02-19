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

#define MARU_API_VALIDATE(fn, ...) \
    do { \
        MARU_Status _res = _maru_validate_##fn(__VA_ARGS__); \
        if (_res != MARU_SUCCESS) return _res; \
    } while (0)

#else
#define MARU_API_VALIDATE(fn, ...) (void)0
#endif

#endif // MARU_API_CONSTRAINTS_H_INCLUDED
