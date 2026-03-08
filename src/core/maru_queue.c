// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "maru/maru_queue.h"
#include "maru_internal.h"
#include "maru_mem_internal.h"
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
    
    MARU_QueueBuffer buffers[2];
} MARU_Queue;

static void _maru_queue_on_event(MARU_EventId type, MARU_Window *window, const MARU_Event *evt, void *userdata) {
    MARU_Queue *q = (MARU_Queue *)userdata;
    if (q->active_count < q->capacity) {
        uint32_t idx = q->active_count++;
        q->active.types[idx] = type;
        q->active.windows[idx] = window;
        q->active.events[idx] = *evt;
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

    buf->types = (MARU_EventId *)((uint8_t *)ptr + types_offset);
    buf->windows = (MARU_Window **)((uint8_t *)ptr + windows_offset);
    buf->events = (MARU_Event *)((uint8_t *)ptr + events_offset);
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

MARU_Status maru_queue_pump(MARU_Queue *queue, uint32_t timeout_ms) {
    if (!queue) return MARU_FAILURE;
    
    return maru_pumpEvents((MARU_Context *)queue->ctx_base, timeout_ms, _maru_queue_on_event, queue);
}

MARU_Status maru_queue_commit(MARU_Queue *queue) {
    if (!queue) return MARU_FAILURE;

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
