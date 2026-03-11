// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru/maru_queue.h"
#include "maru_api_constraints.h"
#include "maru_internal.h"
#include "maru_mem_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct MARU_QueueBuffer {
    MARU_EventId *types;
    MARU_Window **windows;
    MARU_Event *events;
    void *bulk_ptr;
} MARU_QueueBuffer;

typedef struct MARU_Queue {
    MARU_Context_Base *ctx_base;
    
    MARU_QueueBuffer active;
    MARU_QueueBuffer stable;
    
    uint32_t active_count;
    uint32_t stable_count;
    uint32_t capacity;

    MARU_EventMask coalesce_mask;
    uint32_t peak_active_count;
    uint32_t overflow_drop_count;
    uint32_t overflow_compact_count;
    uint32_t overflow_events_compacted;
    uint32_t overflow_drop_count_by_event[MARU_EVENT_USER_15 + 1];
    
    MARU_QueueBuffer buffers[2];
} MARU_Queue;

#define MARU_QUEUE_COMPACT_MAP_CAPACITY 64u
#define MARU_QUEUE_EVENT_BUCKET_COUNT ((uint32_t)MARU_EVENT_USER_15 + 1u)
#define MARU_QUEUE_INVALID_EVENT_ID ((MARU_EventId)-1)

typedef struct MARU_QueueCompactMapEntry {
    MARU_EventId type;
    MARU_Window *window;
    uint32_t survivor_idx;
    bool used;
} MARU_QueueCompactMapEntry;

static void _maru_queue_reset_metrics_internal(MARU_Queue *q) {
    q->peak_active_count = 0;
    q->overflow_drop_count = 0;
    q->overflow_compact_count = 0;
    q->overflow_events_compacted = 0;
    memset(q->overflow_drop_count_by_event, 0, sizeof(q->overflow_drop_count_by_event));
}

static void _maru_queue_update_peak_active_count(MARU_Queue *q) {
    if (q->active_count > q->peak_active_count) {
        q->peak_active_count = q->active_count;
    }
}

static void _maru_queue_count_drop(MARU_Queue *q, MARU_EventId type) {
    q->overflow_drop_count++;

    uint32_t idx = (uint32_t)type;
    if (idx < MARU_QUEUE_EVENT_BUCKET_COUNT) {
        q->overflow_drop_count_by_event[idx]++;
    }
}

static void _maru_queue_coalesce_latest(MARU_EventId type, MARU_Event *dst, const MARU_Event *src) {
    switch (type) {
        case MARU_EVENT_MOUSE_MOVED:
            dst->mouse_motion.position = src->mouse_motion.position;
            dst->mouse_motion.delta.x += src->mouse_motion.delta.x;
            dst->mouse_motion.delta.y += src->mouse_motion.delta.y;
            dst->mouse_motion.raw_delta.x += src->mouse_motion.raw_delta.x;
            dst->mouse_motion.raw_delta.y += src->mouse_motion.raw_delta.y;
            dst->mouse_motion.modifiers = src->mouse_motion.modifiers;
            break;
        case MARU_EVENT_MOUSE_SCROLLED:
            dst->mouse_scroll.delta.x += src->mouse_scroll.delta.x;
            dst->mouse_scroll.delta.y += src->mouse_scroll.delta.y;
            dst->mouse_scroll.steps.x += src->mouse_scroll.steps.x;
            dst->mouse_scroll.steps.y += src->mouse_scroll.steps.y;
            dst->mouse_scroll.modifiers = src->mouse_scroll.modifiers;
            break;
        case MARU_EVENT_WINDOW_RESIZED:
            dst->resized.geometry = src->resized.geometry;
            break;
        default:
            *dst = *src;
            break;
    }
}

static void _maru_queue_fold_older_into_survivor(MARU_EventId type, const MARU_Event *older, MARU_Event *survivor) {
    switch (type) {
        case MARU_EVENT_MOUSE_MOVED:
            survivor->mouse_motion.delta.x += older->mouse_motion.delta.x;
            survivor->mouse_motion.delta.y += older->mouse_motion.delta.y;
            survivor->mouse_motion.raw_delta.x += older->mouse_motion.raw_delta.x;
            survivor->mouse_motion.raw_delta.y += older->mouse_motion.raw_delta.y;
            break;
        case MARU_EVENT_MOUSE_SCROLLED:
            survivor->mouse_scroll.delta.x += older->mouse_scroll.delta.x;
            survivor->mouse_scroll.delta.y += older->mouse_scroll.delta.y;
            survivor->mouse_scroll.steps.x += older->mouse_scroll.steps.x;
            survivor->mouse_scroll.steps.y += older->mouse_scroll.steps.y;
            break;
        case MARU_EVENT_WINDOW_RESIZED:
            // Latest geometry already lives in survivor.
            break;
        default:
            // Latest payload already lives in survivor for generic coalescing.
            break;
    }
}

static uint32_t _maru_queue_compact_hash(MARU_EventId type, MARU_Window *window) {
    uintptr_t window_bits = (uintptr_t)window;
    uint32_t h = (uint32_t)window_bits;
#if UINTPTR_MAX > 0xFFFFFFFFu
    h ^= (uint32_t)(window_bits >> 32u);
#endif
    h ^= (uint32_t)type * 0x9E3779B1u;
    return h;
}

static bool _maru_queue_compact_map_find(const MARU_QueueCompactMapEntry *map, uint32_t cap,
                                         MARU_EventId type, MARU_Window *window, uint32_t *out_survivor_idx) {
    uint32_t idx = _maru_queue_compact_hash(type, window) & (cap - 1u);
    for (uint32_t probe = 0; probe < cap; ++probe) {
        const MARU_QueueCompactMapEntry *entry = &map[idx];
        if (!entry->used) return false;
        if (entry->type == type && entry->window == window) {
            *out_survivor_idx = entry->survivor_idx;
            return true;
        }
        idx = (idx + 1u) & (cap - 1u);
    }
    return false;
}

static bool _maru_queue_compact_map_insert(MARU_QueueCompactMapEntry *map, uint32_t cap,
                                           MARU_EventId type, MARU_Window *window, uint32_t survivor_idx) {
    uint32_t idx = _maru_queue_compact_hash(type, window) & (cap - 1u);
    for (uint32_t probe = 0; probe < cap; ++probe) {
        MARU_QueueCompactMapEntry *entry = &map[idx];
        if (!entry->used) {
            entry->used = true;
            entry->type = type;
            entry->window = window;
            entry->survivor_idx = survivor_idx;
            return true;
        }
        if (entry->type == type && entry->window == window) {
            entry->survivor_idx = survivor_idx;
            return true;
        }
        idx = (idx + 1u) & (cap - 1u);
    }
    return false;
}

static uint32_t _maru_queue_compact_active(MARU_Queue *q) {
    uint32_t count = q->active_count;
    if (count == 0) return 0;

    MARU_QueueCompactMapEntry map[MARU_QUEUE_COMPACT_MAP_CAPACITY];
    memset(map, 0, sizeof(map));
    bool map_saturated = false;
    uint32_t removed = 0;

    for (uint32_t i = count; i-- > 0;) {
        MARU_EventId type = q->active.types[i];
        if (!maru_eventMaskHas(q->coalesce_mask, type)) continue;

        MARU_Window *window = q->active.windows[i];
        uint32_t survivor_idx = UINT32_MAX;
        bool found = _maru_queue_compact_map_find(map, MARU_QUEUE_COMPACT_MAP_CAPACITY, type, window, &survivor_idx);

        if (!found && map_saturated) {
            for (uint32_t j = i + 1u; j < count; ++j) {
                if (q->active.types[j] == type && q->active.windows[j] == window) {
                    survivor_idx = j;
                    found = true;
                    break;
                }
            }
        }

        if (found) {
            _maru_queue_fold_older_into_survivor(type, &q->active.events[i], &q->active.events[survivor_idx]);
            q->active.types[i] = MARU_QUEUE_INVALID_EVENT_ID;
            removed++;
            continue;
        }

        if (!_maru_queue_compact_map_insert(map, MARU_QUEUE_COMPACT_MAP_CAPACITY, type, window, i)) {
            map_saturated = true;
        }
    }

    if (removed == 0) return 0;

    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < count; ++read_idx) {
        if (q->active.types[read_idx] == MARU_QUEUE_INVALID_EVENT_ID) continue;
        if (write_idx != read_idx) {
            q->active.types[write_idx] = q->active.types[read_idx];
            q->active.windows[write_idx] = q->active.windows[read_idx];
            q->active.events[write_idx] = q->active.events[read_idx];
        }
        write_idx++;
    }

    q->active_count = write_idx;
    return removed;
}

static bool _maru_queue_push_internal(MARU_Queue *q, MARU_EventId type,
                                      MARU_Window *window,
                                      const MARU_Event *evt) {
    if (q->active_count > 0 && maru_eventMaskHas(q->coalesce_mask, type)) {
        uint32_t last_idx = q->active_count - 1u;
        if (q->active.types[last_idx] == type && q->active.windows[last_idx] == window) {
            _maru_queue_coalesce_latest(type, &q->active.events[last_idx], evt);
            return true;
        }
    }

    if (q->active_count >= q->capacity) {
        q->overflow_compact_count++;
        q->overflow_events_compacted += _maru_queue_compact_active(q);
    }

    if (q->active_count < q->capacity) {
        uint32_t idx = q->active_count++;
        q->active.types[idx] = type;
        q->active.windows[idx] = window;
        q->active.events[idx] = *evt;
        _maru_queue_update_peak_active_count(q);
        return true;
    } else {
        _maru_queue_count_drop(q, type);
        return false;
    }
}

static bool _maru_queue_buffer_init(MARU_Context_Base *ctx_base, MARU_QueueBuffer *buf, uint32_t capacity) {
    size_t types_size = sizeof(MARU_EventId) * capacity;
    size_t windows_size = sizeof(MARU_Window *) * capacity;
    size_t events_size = sizeof(MARU_Event) * capacity;

    size_t types_offset = 0;
    size_t windows_offset = (types_offset + types_size + 63) & ~(size_t)63;
    size_t events_offset = (windows_offset + windows_size + 63) & ~(size_t)63;
    size_t total_size = events_offset + events_size;

    void *ptr = maru_context_alloc_aligned64(ctx_base, total_size);
    if (!ptr) {
        return false;
    }

    buf->types = (MARU_EventId *)(void *)((uint8_t *)ptr + types_offset);
    buf->windows = (MARU_Window **)(void *)((uint8_t *)ptr + windows_offset);
    buf->events = (MARU_Event *)(void *)((uint8_t *)ptr + events_offset);
    buf->bulk_ptr = ptr;
    
    return true;
}

static void _maru_queue_buffer_cleanup(MARU_Context_Base *ctx_base, MARU_QueueBuffer *buf) {
    if (buf->bulk_ptr) {
        maru_context_free_aligned64(ctx_base, buf->bulk_ptr);
        buf->bulk_ptr = NULL;
    }
}

MARU_Status maru_queue_create(MARU_Context *ctx, uint32_t capacity, MARU_Queue **out_queue) {
    if (!ctx || !out_queue || capacity == 0) {
        return MARU_FAILURE;
    }

    MARU_Context_Base *ctx_base = (MARU_Context_Base *)ctx;
    MARU_Queue *q = maru_context_alloc(ctx_base, sizeof(MARU_Queue));
    if (!q) {
        return MARU_FAILURE;
    }

    q->ctx_base = ctx_base;
    q->capacity = capacity;
    q->active_count = 0;
    q->stable_count = 0;
    q->coalesce_mask = 0;
    _maru_queue_reset_metrics_internal(q);

    if (!_maru_queue_buffer_init(ctx_base, &q->buffers[0], capacity)) {
        maru_context_free(ctx_base, q);
        return MARU_FAILURE;
    }

    if (!_maru_queue_buffer_init(ctx_base, &q->buffers[1], capacity)) {
        _maru_queue_buffer_cleanup(ctx_base, &q->buffers[0]);
        maru_context_free(ctx_base, q);
        return MARU_FAILURE;
    }

    q->active = q->buffers[0];
    q->stable = q->buffers[1];

    *out_queue = q;
    return MARU_SUCCESS;
}

void maru_queue_destroy(MARU_Queue *queue) {
    if (!queue) return;
    
    MARU_Context_Base *ctx_base = queue->ctx_base;
    _maru_queue_buffer_cleanup(ctx_base, &queue->buffers[0]);
    _maru_queue_buffer_cleanup(ctx_base, &queue->buffers[1]);
    maru_context_free(ctx_base, queue);
}

MARU_Status maru_queue_push(MARU_Queue *queue, MARU_EventId type,
                            MARU_Window *window, const MARU_Event *event) {
    if (!queue || !event) return MARU_FAILURE;

#ifdef MARU_VALIDATE_API_CALLS
    _maru_validate_thread(queue->ctx_base);
    MARU_CONSTRAINT_CHECK(maru_isKnownEventId(type));
    MARU_CONSTRAINT_CHECK(maru_isQueueSafeEventId(type));
#endif

    return _maru_queue_push_internal(queue, type, window, event)
               ? MARU_SUCCESS
               : MARU_FAILURE;
}

MARU_Status maru_queue_commit(MARU_Queue *queue) {
    if (!queue) return MARU_FAILURE;

#ifdef MARU_VALIDATE_API_CALLS
    _maru_validate_thread(queue->ctx_base);
#endif

    // O(1) swap
    MARU_QueueBuffer tmp_buf = queue->active;
    queue->stable = tmp_buf;
    queue->stable_count = queue->active_count;

    // Use the other buffer for next active collection
    queue->active = (tmp_buf.types == queue->buffers[0].types) ? queue->buffers[1] : queue->buffers[0];
    queue->active_count = 0;

    return MARU_SUCCESS;
}

void maru_queue_scan(MARU_Queue *queue, MARU_EventMask mask, MARU_EventCallback callback, void *userdata) {
    if (!queue || !callback) return;

    MARU_QueueBuffer stable = queue->stable;
    uint32_t count = queue->stable_count;

    for (uint32_t i = 0; i < count; ++i) {
        if (maru_eventMaskHas(mask, stable.types[i])) {
            callback(stable.types[i], stable.windows[i], &stable.events[i], userdata);
        }
    }
}

void maru_queue_set_coalesce_mask(MARU_Queue *queue, MARU_EventMask mask) {
    if (queue) {
        queue->coalesce_mask = mask;
    }
}

void maru_queue_get_metrics(const MARU_Queue *queue, MARU_QueueMetrics *out_metrics) {
    if (!out_metrics) return;

    if (!queue) {
        memset(out_metrics, 0, sizeof(*out_metrics));
        return;
    }

    out_metrics->peak_active_count = queue->peak_active_count;
    out_metrics->overflow_drop_count = queue->overflow_drop_count;
    out_metrics->overflow_compact_count = queue->overflow_compact_count;
    out_metrics->overflow_events_compacted = queue->overflow_events_compacted;
    memcpy(out_metrics->overflow_drop_count_by_event,
           queue->overflow_drop_count_by_event,
           sizeof(out_metrics->overflow_drop_count_by_event));
}

void maru_queue_reset_metrics(MARU_Queue *queue) {
    if (!queue) return;
    _maru_queue_reset_metrics_internal(queue);
}
