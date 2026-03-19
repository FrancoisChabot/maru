// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "internal_event_queue.h"
#include "maru_mem_internal.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>

bool _maru_internal_event_queue_init(MARU_InternalEventQueue *q,
                                     MARU_Context_Base *ctx,
                                     uint32_t capacity_power_of_2) {
  if (!q) return false;
  
  q->capacity = capacity_power_of_2;
  q->mask = capacity_power_of_2 - 1;
  atomic_init(&q->head, 0);
  atomic_init(&q->tail, 0);

  q->buffer = maru_context_alloc_aligned64(
      ctx, sizeof(MARU_InternalQueuedEvent) * capacity_power_of_2);
  if (!q->buffer) return false;

  memset(q->buffer, 0, sizeof(MARU_InternalQueuedEvent) * capacity_power_of_2);
  for (uint32_t i = 0; i < capacity_power_of_2; ++i) {
    atomic_init(&q->buffer[i].state, 0);
  }
  return true;
}

void _maru_internal_event_queue_cleanup(MARU_InternalEventQueue *q,
                                        MARU_Context_Base *ctx) {
  if (q->buffer) {
    for (uint32_t i = 0; i < q->capacity; ++i) {
      MARU_InternalQueuedEvent *slot = &q->buffer[i];
      if (atomic_load_explicit(&slot->state, memory_order_acquire) == 2 &&
          slot->cleanup_cb) {
        slot->cleanup_cb(ctx, slot->cleanup_userdata);
      }
    }
  }
  if (q->buffer) {
    maru_context_free_aligned64(ctx, q->buffer);
    q->buffer = NULL;
  }
}

bool _maru_internal_event_queue_push(MARU_InternalEventQueue *q,
                                     MARU_EventId type,
                                     MARU_Window *window,
                                     MARU_Event evt,
                                     MARU_InternalQueuedEventCleanupFn cleanup_cb,
                                     void *cleanup_userdata) {
  if (!q || !q->buffer || q->capacity == 0) return false;

  size_t head, tail;
  
  do {
    head = atomic_load_explicit(&q->head, memory_order_relaxed);
    tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    
    if (head - tail >= q->capacity) {
      return false; // Full
    }
  } while (!atomic_compare_exchange_weak_explicit(&q->head, &head, head + 1, 
                                                  memory_order_relaxed, memory_order_relaxed));

  // We reserved the slot at 'head'
  MARU_InternalQueuedEvent *slot = &q->buffer[head & q->mask];

  // Wait until the slot is truly free (state 0).
  // In a correct MPSC with head/tail check above, this should be almost immediate,
  // unless the consumer just read it but hasn't set state to 0 yet (which implies tail wasn't incremented?)
  // Actually, consumer sets state to 0 THEN increments tail.
  // So if head < tail + capacity, then slot MUST be 0.
  // We can assert or just proceed.
  // Let's verify:
  // Consumer: 
  //   Load state. If 2: read. Store state 0. Store tail++.
  // Producer:
  //   Load tail. If head < tail + cap: CAS head++. 
  //   Since tail is only incremented AFTER state=0, we are safe.
  
  slot->type = type;
  slot->window = window;
  slot->evt = evt;
  slot->cleanup_cb = cleanup_cb;
  slot->cleanup_userdata = cleanup_userdata;
  
  // Publish
  atomic_store_explicit(&slot->state, 2, memory_order_release);
  return true;
}

bool _maru_internal_event_queue_pop(MARU_InternalEventQueue *q,
                                    MARU_EventId *out_type,
                                    MARU_Window **out_window,
                                    MARU_Event *out_evt,
                                    MARU_InternalQueuedEventCleanupFn *out_cleanup_cb,
                                    void **out_cleanup_userdata) {
  if (!q || !q->buffer || q->capacity == 0) return false;

  size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
  MARU_InternalQueuedEvent *slot = &q->buffer[tail & q->mask];

  int state = atomic_load_explicit(&slot->state, memory_order_acquire);
  if (state != 2) {
    return false; // Empty or writing in progress
  }

  *out_type = slot->type;
  *out_window = slot->window;
  *out_evt = slot->evt;
  if (out_cleanup_cb) {
    *out_cleanup_cb = slot->cleanup_cb;
  }
  if (out_cleanup_userdata) {
    *out_cleanup_userdata = slot->cleanup_userdata;
  }
  slot->cleanup_cb = NULL;
  slot->cleanup_userdata = NULL;

  atomic_store_explicit(&slot->state, 0, memory_order_release);
  atomic_store_explicit(&q->tail, tail + 1, memory_order_release);
  
  return true;
}
