#ifndef MARU_TEST_UTILS_H_INCLUDED
#define MARU_TEST_UTILS_H_INCLUDED

#include "maru/maru.h"
#include <stdbool.h>
#include "maru_internal.h"
#include "maru_mem_internal.h"
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __cplusplus
// maru_internal.h relies on C11 _Atomic and is not C++-friendly.
// Integration tests built as C++ only need the tracking allocator utilities.
#else
#include "maru_internal.h"
#include "maru_mem_internal.h"
#endif


typedef struct MARU_TestTrackedAllocation {
    void *ptr;
    size_t size;
    struct MARU_TestTrackedAllocation *next;
} MARU_TestTrackedAllocation;

typedef struct MARU_TestTrackingAllocator {
    MARU_Allocator allocator;
    MARU_TestTrackedAllocation *head;
    size_t live_alloc_count;
    size_t live_alloc_bytes;
    size_t total_alloc_count;
    size_t total_free_count;
    size_t alloc_event_count;
    bool oom_enabled;
    size_t oom_fail_at_event;
    bool oom_triggered;
    bool tracking_error;
} MARU_TestTrackingAllocator;

static inline MARU_TestTrackedAllocation *
_maru_test_tracking_find_allocation(MARU_TestTrackingAllocator *tracking,
                                    void *ptr,
                                    MARU_TestTrackedAllocation ***out_prev_next) {
    if (out_prev_next) {
        *out_prev_next = NULL;
    }
    if (!tracking || !ptr) {
        return NULL;
    }

    MARU_TestTrackedAllocation **link = &tracking->head;
    while (*link) {
        if ((*link)->ptr == ptr) {
            if (out_prev_next) {
                *out_prev_next = link;
            }
            return *link;
        }
        link = &(*link)->next;
    }

    return NULL;
}

static inline void *_maru_test_tracking_alloc(size_t size, void *userdata) {
    MARU_TestTrackingAllocator *tracking = (MARU_TestTrackingAllocator *)userdata;
    if (!tracking) {
        return NULL;
    }

    tracking->alloc_event_count++;
    if (tracking->oom_enabled &&
        tracking->alloc_event_count == tracking->oom_fail_at_event) {
        tracking->oom_triggered = true;
        return NULL;
    }

    void *ptr = malloc(size);
    if (!ptr) {
        return NULL;
    }

    MARU_TestTrackedAllocation *node =
        (MARU_TestTrackedAllocation *)malloc(sizeof(MARU_TestTrackedAllocation));
    if (!node) {
        free(ptr);
        tracking->tracking_error = true;
        return NULL;
    }

    node->ptr = ptr;
    node->size = size;
    node->next = tracking->head;
    tracking->head = node;

    tracking->live_alloc_count++;
    tracking->live_alloc_bytes += size;
    tracking->total_alloc_count++;
    return ptr;
}

static inline void *_maru_test_tracking_realloc(void *ptr, size_t new_size, void *userdata) {
    MARU_TestTrackingAllocator *tracking = (MARU_TestTrackingAllocator *)userdata;
    if (!tracking) {
        return NULL;
    }

    if (!ptr) {
        return _maru_test_tracking_alloc(new_size, userdata);
    }
    if (new_size == 0u) {
        // Behave like free() for allocator tracking.
        MARU_TestTrackedAllocation **prev_next = NULL;
        MARU_TestTrackedAllocation *node =
            _maru_test_tracking_find_allocation(tracking, ptr, &prev_next);
        if (node && prev_next) {
            *prev_next = node->next;
            if (tracking->live_alloc_count > 0u) {
                tracking->live_alloc_count--;
            }
            if (tracking->live_alloc_bytes >= node->size) {
                tracking->live_alloc_bytes -= node->size;
            } else {
                tracking->live_alloc_bytes = 0u;
                tracking->tracking_error = true;
            }
            free(node);
        } else {
            tracking->tracking_error = true;
        }
        free(ptr);
        tracking->total_free_count++;
        return NULL;
    }

    tracking->alloc_event_count++;
    if (tracking->oom_enabled &&
        tracking->alloc_event_count == tracking->oom_fail_at_event) {
        tracking->oom_triggered = true;
        return NULL;
    }

    MARU_TestTrackedAllocation **prev_next = NULL;
    MARU_TestTrackedAllocation *node =
        _maru_test_tracking_find_allocation(tracking, ptr, &prev_next);

    void *new_ptr = realloc(ptr, new_size);
    if (!new_ptr) {
        return NULL;
    }

    if (!node) {
        node = (MARU_TestTrackedAllocation *)malloc(sizeof(MARU_TestTrackedAllocation));
        if (!node) {
            tracking->tracking_error = true;
            return new_ptr;
        }
        node->ptr = new_ptr;
        node->size = new_size;
        node->next = tracking->head;
        tracking->head = node;
        tracking->live_alloc_count++;
        tracking->live_alloc_bytes += new_size;
        tracking->total_alloc_count++;
        tracking->tracking_error = true;
        return new_ptr;
    }

    if (tracking->live_alloc_bytes >= node->size) {
        tracking->live_alloc_bytes -= node->size;
    } else {
        tracking->live_alloc_bytes = 0u;
        tracking->tracking_error = true;
    }
    tracking->live_alloc_bytes += new_size;
    node->ptr = new_ptr;
    node->size = new_size;
    return new_ptr;
}

static inline void _maru_test_tracking_free(void *ptr, void *userdata) {
    MARU_TestTrackingAllocator *tracking = (MARU_TestTrackingAllocator *)userdata;
    if (!ptr) {
        return;
    }

    if (!tracking) {
        free(ptr);
        return;
    }

    MARU_TestTrackedAllocation **prev_next = NULL;
    MARU_TestTrackedAllocation *node =
        _maru_test_tracking_find_allocation(tracking, ptr, &prev_next);
    if (!node || !prev_next) {
        tracking->tracking_error = true;
        free(ptr);
        tracking->total_free_count++;
        return;
    }

    *prev_next = node->next;
    if (tracking->live_alloc_count > 0u) {
        tracking->live_alloc_count--;
    }
    if (tracking->live_alloc_bytes >= node->size) {
        tracking->live_alloc_bytes -= node->size;
    } else {
        tracking->live_alloc_bytes = 0u;
        tracking->tracking_error = true;
    }
    free(node);
    free(ptr);
    tracking->total_free_count++;
}

static inline void
maru_test_tracking_allocator_init(MARU_TestTrackingAllocator *tracking) {
    if (!tracking) {
        return;
    }
    memset(tracking, 0, sizeof(*tracking));
    tracking->allocator.alloc_cb = _maru_test_tracking_alloc;
    tracking->allocator.realloc_cb = _maru_test_tracking_realloc;
    tracking->allocator.free_cb = _maru_test_tracking_free;
    tracking->allocator.userdata = tracking;
}

static inline void
maru_test_tracking_allocator_apply(MARU_TestTrackingAllocator *tracking,
                                   MARU_ContextCreateInfo *create_info) {
    if (!tracking || !create_info) {
        return;
    }
    create_info->allocator = tracking->allocator;
}

static inline void
maru_test_tracking_allocator_set_oom(MARU_TestTrackingAllocator *tracking,
                                     bool enabled,
                                     size_t fail_at_event) {
    if (!tracking) {
        return;
    }
    tracking->oom_enabled = enabled;
    tracking->oom_fail_at_event = fail_at_event;
    tracking->oom_triggered = false;
}

static inline size_t
maru_test_tracking_allocator_get_alloc_event_count(const MARU_TestTrackingAllocator *tracking) {
    if (!tracking) {
        return 0u;
    }
    return tracking->alloc_event_count;
}

static inline bool
maru_test_tracking_allocator_is_clean(const MARU_TestTrackingAllocator *tracking) {
    if (!tracking) {
        return false;
    }
    return !tracking->tracking_error && tracking->live_alloc_count == 0u &&
           tracking->live_alloc_bytes == 0u;
}

static inline void
maru_test_tracking_allocator_shutdown(MARU_TestTrackingAllocator *tracking) {
    if (!tracking) {
        return;
    }
    MARU_TestTrackedAllocation *it = tracking->head;
    tracking->head = NULL;
    while (it) {
        MARU_TestTrackedAllocation *next = it->next;
        free(it->ptr);
        free(it);
        it = next;
    }
    tracking->live_alloc_count = 0u;
    tracking->live_alloc_bytes = 0u;
}

#ifndef __cplusplus
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
#endif  // !__cplusplus

#ifdef __cplusplus
}
#endif

#endif // MARU_TEST_UTILS_H_INCLUDED
