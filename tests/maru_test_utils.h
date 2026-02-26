#ifndef MARU_TEST_UTILS_H_INCLUDED
#define MARU_TEST_UTILS_H_INCLUDED

#include "maru/maru.h"
#include "maru_internal.h"
#include "maru_mem_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

// A dummy destroy function for the mock backend
static inline MARU_Status _mock_destroyContext(MARU_Context* context) {
    MARU_Context_Base* ctx_base = (MARU_Context_Base*)context;
    _maru_cleanup_context_base(ctx_base);
    maru_context_free(ctx_base, context);
    return MARU_SUCCESS;
}

static inline MARU_Status _mock_updateContext(MARU_Context* context, uint64_t field_mask,
                                              const MARU_ContextAttributes* attributes) {
    _maru_update_context_base((MARU_Context_Base*)context, field_mask, attributes);
    return MARU_SUCCESS;
}

static inline MARU_Status _mock_wakeContext(MARU_Context* context) {
    (void)context;
    return MARU_SUCCESS;
}

#ifdef MARU_INDIRECT_BACKEND
static const MARU_Backend _maru_mock_backend = {
    .destroyContext = _mock_destroyContext,
    .updateContext = _mock_updateContext,

    .pumpEvents = NULL,
    .wakeContext = _mock_wakeContext,
    
    .createWindow = NULL,
    .destroyWindow = NULL,
    .getWindowGeometry = NULL,
    .updateWindow = NULL,
    .requestWindowFocus = NULL,
    .requestWindowFrame = NULL,
    .createCursor = NULL,
    .destroyCursor = NULL,

    .getMonitorModes = NULL,
    .setMonitorMode = NULL,
    .resetMonitorMetrics = NULL,
    .getContextNativeHandle = NULL,
    .getWindowNativeHandle = NULL,
};
#endif

/** @brief Manually bootstraps a MARU_Context_Base for core testing. */
static inline MARU_Context* maru_test_createContext(const MARU_ContextCreateInfo* create_info) {
    MARU_Context_Base* ctx = (MARU_Context_Base*)maru_context_alloc_bootstrap(
        create_info, sizeof(MARU_Context_Base));
    
    if (!ctx) return NULL;

    if (create_info->allocator.alloc_cb) {
        ctx->allocator = create_info->allocator;
    } else {
        ctx->allocator.alloc_cb = _maru_default_alloc;
        ctx->allocator.realloc_cb = _maru_default_realloc;
        ctx->allocator.free_cb = _maru_default_free;
        ctx->allocator.userdata = NULL;
    }

    ctx->pub.flags = MARU_CONTEXT_STATE_READY;
    ctx->tuning = create_info->tuning;
    ctx->diagnostic_cb = create_info->attributes.diagnostic_cb;
    ctx->diagnostic_userdata = create_info->attributes.diagnostic_userdata;

    _maru_init_context_base(ctx);
#ifdef MARU_GATHER_METRICS
    _maru_update_mem_metrics_alloc(ctx, sizeof(MARU_Context_Base));
#endif

#ifdef MARU_INDIRECT_BACKEND
    ctx->backend = &_maru_mock_backend;
#endif

#ifdef MARU_VALIDATE_API_CALLS
    extern MARU_ThreadId _maru_getCurrentThreadId(void);
    ctx->creator_thread = _maru_getCurrentThreadId();
#endif

    return (MARU_Context*)ctx;
}

#ifdef __cplusplus
}
#endif

#endif // MARU_TEST_UTILS_H_INCLUDED
