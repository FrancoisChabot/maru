// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru/maru.h"
#include "maru_api_constraints.h"
#include "maru_internal.h"
#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct MARU_QueueBuffer {
    MARU_EventId *types;
    MARU_WindowId *window_ids;
    MARU_Event *events;
    void *bulk_ptr;
} MARU_QueueBuffer;

typedef struct MARU_Queue {
    MARU_Allocator allocator;
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
#ifdef MARU_VALIDATE_API_CALLS
    MARU_ThreadId creator_thread;
#endif
} MARU_Queue;

#define MARU_QUEUE_COMPACT_MAP_CAPACITY 64u
#define MARU_QUEUE_EVENT_BUCKET_COUNT ((uint32_t)MARU_EVENT_USER_15 + 1u)
#define MARU_QUEUE_INVALID_EVENT_ID ((MARU_EventId)-1)

typedef struct MARU_QueueCompactMapEntry {
    MARU_EventId type;
    MARU_WindowId window_id;
    uint32_t survivor_idx;
    bool used;
} MARU_QueueCompactMapEntry;

static MARU_Allocator _maru_queue_resolve_allocator(const MARU_Allocator *allocator) {
    if (allocator && allocator->alloc_cb) {
        return *allocator;
    }
    return (MARU_Allocator){
        .alloc_cb = _maru_default_alloc,
        .realloc_cb = _maru_default_realloc,
        .free_cb = _maru_default_free,
        .userdata = NULL,
    };
}

static void *_maru_queue_alloc_raw(const MARU_Allocator *allocator, size_t size) {
    return allocator->alloc_cb(size, allocator->userdata);
}

static void _maru_queue_free_raw(const MARU_Allocator *allocator, void *ptr) {
    if (ptr) {
        allocator->free_cb(ptr, allocator->userdata);
    }
}

static void *_maru_queue_alloc_aligned64(const MARU_Allocator *allocator, size_t size) {
    const size_t alignment = 64u;
    void *ptr = _maru_queue_alloc_raw(allocator, size + alignment + sizeof(void *));
    if (!ptr) {
        return NULL;
    }

    size_t addr = (size_t)ptr + sizeof(void *);
    void *aligned_ptr = (void *)((addr + alignment - 1u) & ~(alignment - 1u));
    ((void **)aligned_ptr)[-1] = ptr;
    return aligned_ptr;
}

static void _maru_queue_free_aligned64(const MARU_Allocator *allocator, void *ptr) {
    if (ptr) {
        void *original_ptr = ((void **)ptr)[-1];
        _maru_queue_free_raw(allocator, original_ptr);
    }
}

#ifdef MARU_VALIDATE_API_CALLS
static void _maru_queue_validate_thread(const MARU_Queue *queue) {
    MARU_CONSTRAINT_CHECK(queue->creator_thread == _maru_getCurrentThreadId());
}
#else
static void _maru_queue_validate_thread(const MARU_Queue *queue) {
    (void)queue;
}
#endif

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
            dst->mouse_moved.dip_position = src->mouse_moved.dip_position;
            dst->mouse_moved.dip_delta.x += src->mouse_moved.dip_delta.x;
            dst->mouse_moved.dip_delta.y += src->mouse_moved.dip_delta.y;
            dst->mouse_moved.raw_dip_delta.x += src->mouse_moved.raw_dip_delta.x;
            dst->mouse_moved.raw_dip_delta.y += src->mouse_moved.raw_dip_delta.y;
            dst->mouse_moved.modifiers = src->mouse_moved.modifiers;
            break;
        case MARU_EVENT_MOUSE_SCROLLED:
            dst->mouse_scrolled.dip_delta.x += src->mouse_scrolled.dip_delta.x;
            dst->mouse_scrolled.dip_delta.y += src->mouse_scrolled.dip_delta.y;
            dst->mouse_scrolled.steps.x += src->mouse_scrolled.steps.x;
            dst->mouse_scrolled.steps.y += src->mouse_scrolled.steps.y;
            dst->mouse_scrolled.modifiers = src->mouse_scrolled.modifiers;
            break;
        case MARU_EVENT_WINDOW_RESIZED:
            dst->window_resized.geometry = src->window_resized.geometry;
            break;
        default:
            *dst = *src;
            break;
    }
}

static void _maru_queue_fold_older_into_survivor(MARU_EventId type, const MARU_Event *older, MARU_Event *survivor) {
    switch (type) {
        case MARU_EVENT_MOUSE_MOVED:
            survivor->mouse_moved.dip_delta.x += older->mouse_moved.dip_delta.x;
            survivor->mouse_moved.dip_delta.y += older->mouse_moved.dip_delta.y;
            survivor->mouse_moved.raw_dip_delta.x += older->mouse_moved.raw_dip_delta.x;
            survivor->mouse_moved.raw_dip_delta.y += older->mouse_moved.raw_dip_delta.y;
            break;
        case MARU_EVENT_MOUSE_SCROLLED:
            survivor->mouse_scrolled.dip_delta.x += older->mouse_scrolled.dip_delta.x;
            survivor->mouse_scrolled.dip_delta.y += older->mouse_scrolled.dip_delta.y;
            survivor->mouse_scrolled.steps.x += older->mouse_scrolled.steps.x;
            survivor->mouse_scrolled.steps.y += older->mouse_scrolled.steps.y;
            break;
        case MARU_EVENT_WINDOW_RESIZED:
            // Latest geometry already lives in survivor.
            break;
        default:
            // Latest payload already lives in survivor for generic coalescing.
            break;
    }
}

static uint32_t _maru_queue_compact_hash(MARU_EventId type,
                                         MARU_WindowId window_id) {
    uint64_t id_bits = (uint64_t)window_id;
    uint32_t h = (uint32_t)id_bits;
    h ^= (uint32_t)(id_bits >> 32u);
    h ^= (uint32_t)type * 0x9E3779B1u;
    return h;
}

static bool _maru_queue_compact_map_find(const MARU_QueueCompactMapEntry *map, uint32_t cap,
                                         MARU_EventId type,
                                         MARU_WindowId window_id,
                                         uint32_t *out_survivor_idx) {
    uint32_t idx = _maru_queue_compact_hash(type, window_id) & (cap - 1u);
    for (uint32_t probe = 0; probe < cap; ++probe) {
        const MARU_QueueCompactMapEntry *entry = &map[idx];
        if (!entry->used) return false;
        if (entry->type == type && entry->window_id == window_id) {
            *out_survivor_idx = entry->survivor_idx;
            return true;
        }
        idx = (idx + 1u) & (cap - 1u);
    }
    return false;
}

static bool _maru_queue_compact_map_insert(MARU_QueueCompactMapEntry *map, uint32_t cap,
                                           MARU_EventId type,
                                           MARU_WindowId window_id,
                                           uint32_t survivor_idx) {
    uint32_t idx = _maru_queue_compact_hash(type, window_id) & (cap - 1u);
    for (uint32_t probe = 0; probe < cap; ++probe) {
        MARU_QueueCompactMapEntry *entry = &map[idx];
        if (!entry->used) {
            entry->used = true;
            entry->type = type;
            entry->window_id = window_id;
            entry->survivor_idx = survivor_idx;
            return true;
        }
        if (entry->type == type && entry->window_id == window_id) {
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

        MARU_WindowId window_id = q->active.window_ids[i];
        uint32_t survivor_idx = UINT32_MAX;
        bool found = _maru_queue_compact_map_find(
            map, MARU_QUEUE_COMPACT_MAP_CAPACITY, type, window_id,
            &survivor_idx);

        if (!found && map_saturated) {
            for (uint32_t j = i + 1u; j < count; ++j) {
                if (q->active.types[j] == type &&
                    q->active.window_ids[j] == window_id) {
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

        if (!_maru_queue_compact_map_insert(
                map, MARU_QUEUE_COMPACT_MAP_CAPACITY, type, window_id, i)) {
            map_saturated = true;
        }
    }

    if (removed == 0) return 0;

    uint32_t write_idx = 0;
    for (uint32_t read_idx = 0; read_idx < count; ++read_idx) {
        if (q->active.types[read_idx] == MARU_QUEUE_INVALID_EVENT_ID) continue;
        if (write_idx != read_idx) {
            q->active.types[write_idx] = q->active.types[read_idx];
            q->active.window_ids[write_idx] = q->active.window_ids[read_idx];
            q->active.events[write_idx] = q->active.events[read_idx];
        }
        write_idx++;
    }

    q->active_count = write_idx;
    return removed;
}

static bool _maru_queue_push_internal(MARU_Queue *q, MARU_EventId type,
                                      MARU_WindowId window_id,
                                      const MARU_Event *evt) {
    if (q->active_count > 0 && maru_eventMaskHas(q->coalesce_mask, type)) {
        uint32_t last_idx = q->active_count - 1u;
        if (q->active.types[last_idx] == type &&
            q->active.window_ids[last_idx] == window_id) {
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
        q->active.window_ids[idx] = window_id;
        q->active.events[idx] = *evt;
        _maru_queue_update_peak_active_count(q);
        return true;
    } else {
        _maru_queue_count_drop(q, type);
        return false;
    }
}

static bool _maru_queue_buffer_init(const MARU_Allocator *allocator, MARU_QueueBuffer *buf, uint32_t capacity) {
    size_t types_size = sizeof(MARU_EventId) * capacity;
    size_t window_ids_size = sizeof(MARU_WindowId) * capacity;
    size_t events_size = sizeof(MARU_Event) * capacity;

    size_t types_offset = 0;
    size_t window_ids_offset = (types_offset + types_size + 63) & ~(size_t)63;
    size_t events_offset =
        (window_ids_offset + window_ids_size + 63) & ~(size_t)63;
    size_t total_size = events_offset + events_size;

    void *ptr = _maru_queue_alloc_aligned64(allocator, total_size);
    if (!ptr) {
        return false;
    }

    buf->types = (MARU_EventId *)(void *)((uint8_t *)ptr + types_offset);
    buf->window_ids =
        (MARU_WindowId *)(void *)((uint8_t *)ptr + window_ids_offset);
    buf->events = (MARU_Event *)(void *)((uint8_t *)ptr + events_offset);
    buf->bulk_ptr = ptr;
    
    return true;
}

static void _maru_queue_buffer_cleanup(const MARU_Allocator *allocator, MARU_QueueBuffer *buf) {
    if (buf->bulk_ptr) {
        _maru_queue_free_aligned64(allocator, buf->bulk_ptr);
        buf->bulk_ptr = NULL;
    }
}

MARU_Status maru_createQueue(const MARU_QueueCreateInfo *create_info, MARU_Queue **out_queue) {
    MARU_API_VALIDATE(createQueue, create_info, out_queue);

    if (!create_info || !out_queue || create_info->capacity == 0u) {
        return MARU_FAILURE;
    }
    const bool any_custom = create_info->allocator.alloc_cb != NULL || create_info->allocator.realloc_cb != NULL || create_info->allocator.free_cb != NULL;
    if (any_custom && (create_info->allocator.alloc_cb == NULL || create_info->allocator.realloc_cb == NULL || create_info->allocator.free_cb == NULL)) {
        return MARU_FAILURE;
    }

    MARU_Allocator allocator = _maru_queue_resolve_allocator(&create_info->allocator);
    MARU_Queue *q = (MARU_Queue *)_maru_queue_alloc_raw(&allocator, sizeof(MARU_Queue));
    if (!q) {
        return MARU_FAILURE;
    }

    memset(q, 0, sizeof(*q));
    q->allocator = allocator;
    q->capacity = create_info->capacity;
    q->active_count = 0;
    q->stable_count = 0;
    q->coalesce_mask = 0;
    _maru_queue_reset_metrics_internal(q);
#ifdef MARU_VALIDATE_API_CALLS
    q->creator_thread = _maru_getCurrentThreadId();
#endif

    if (!_maru_queue_buffer_init(&q->allocator, &q->buffers[0], q->capacity)) {
        _maru_queue_free_raw(&q->allocator, q);
        return MARU_FAILURE;
    }

    if (!_maru_queue_buffer_init(&q->allocator, &q->buffers[1], q->capacity)) {
        _maru_queue_buffer_cleanup(&q->allocator, &q->buffers[0]);
        _maru_queue_free_raw(&q->allocator, q);
        return MARU_FAILURE;
    }

    q->active = q->buffers[0];
    q->stable = q->buffers[1];

    *out_queue = q;
    return MARU_SUCCESS;
}

void maru_destroyQueue(MARU_Queue *queue) {
    MARU_API_VALIDATE(destroyQueue, queue);
    if (!queue) return;

    _maru_queue_validate_thread(queue);
    MARU_Allocator allocator = queue->allocator;
    _maru_queue_buffer_cleanup(&queue->allocator, &queue->buffers[0]);
    _maru_queue_buffer_cleanup(&queue->allocator, &queue->buffers[1]);
    _maru_queue_free_raw(&allocator, queue);
}

MARU_Status maru_pushQueue(MARU_Queue *queue, MARU_EventId type,
                           MARU_WindowId window_id, const MARU_Event *event) {
    MARU_API_VALIDATE(pushQueue, queue, type, window_id, event);

    if (!queue || !event) return MARU_FAILURE;
    _maru_queue_validate_thread(queue);

    return _maru_queue_push_internal(queue, type, window_id, event)
               ? MARU_SUCCESS
               : MARU_FAILURE;
}

MARU_Status maru_commitQueue(MARU_Queue *queue) {
    MARU_API_VALIDATE(commitQueue, queue);

    if (!queue) return MARU_FAILURE;
    _maru_queue_validate_thread(queue);

    // O(1) swap
    MARU_QueueBuffer tmp_buf = queue->active;
    queue->stable = tmp_buf;
    queue->stable_count = queue->active_count;

    // Use the other buffer for next active collection
    queue->active = (tmp_buf.types == queue->buffers[0].types) ? queue->buffers[1] : queue->buffers[0];
    queue->active_count = 0;

    return MARU_SUCCESS;
}

void maru_scanQueue(MARU_Queue *queue,
                    MARU_EventMask mask,
                    MARU_QueueEventCallback callback,
                    void *userdata) {
    MARU_API_VALIDATE(scanQueue, queue, mask, callback, userdata);
    if (!queue || !callback) return;
    _maru_queue_validate_thread(queue);

    MARU_QueueBuffer stable = queue->stable;
    uint32_t count = queue->stable_count;

    for (uint32_t i = 0; i < count; ++i) {
        if (maru_eventMaskHas(mask, stable.types[i])) {
            callback(stable.types[i], stable.window_ids[i], &stable.events[i],
                     userdata);
        }
    }
}

void maru_setQueueCoalesceMask(MARU_Queue *queue, MARU_EventMask mask) {
    MARU_API_VALIDATE(setQueueCoalesceMask, queue, mask);
    if (queue) {
        _maru_queue_validate_thread(queue);
        queue->coalesce_mask = mask;
    }
}

void maru_getQueueMetrics(const MARU_Queue *queue, MARU_QueueMetrics *out_metrics) {
    MARU_API_VALIDATE(getQueueMetrics, queue, out_metrics);
    if (!out_metrics) return;

    if (!queue) {
        memset(out_metrics, 0, sizeof(*out_metrics));
        return;
    }
    _maru_queue_validate_thread(queue);

    out_metrics->peak_active_count = queue->peak_active_count;
    out_metrics->overflow_drop_count = queue->overflow_drop_count;
    out_metrics->overflow_compact_count = queue->overflow_compact_count;
    out_metrics->overflow_events_compacted = queue->overflow_events_compacted;
    memcpy(out_metrics->overflow_drop_count_by_event,
           queue->overflow_drop_count_by_event,
           sizeof(out_metrics->overflow_drop_count_by_event));
}

void maru_resetQueueMetrics(MARU_Queue *queue) {
    MARU_API_VALIDATE(resetQueueMetrics, queue);
    if (!queue) return;
    _maru_queue_validate_thread(queue);
    _maru_queue_reset_metrics_internal(queue);
}
