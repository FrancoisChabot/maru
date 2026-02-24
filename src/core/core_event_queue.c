// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#include "core_event_queue.h"
#include "maru_mem_internal.h"
#include <stdatomic.h>
#include <string.h>

bool _maru_event_queue_init(MARU_EventQueue *q, MARU_Context_Base *ctx, uint32_t capacity_power_of_2) {
  if (!q) return false;
  
  q->capacity = capacity_power_of_2;
  q->mask = capacity_power_of_2 - 1;
  atomic_init(&q->head, 0);
  atomic_init(&q->tail, 0);
#ifdef MARU_GATHER_METRICS
  atomic_init(&q->high_watermark, 0);
  atomic_init(&q->drop_count, 0);
#endif

  q->buffer = maru_context_alloc(ctx, sizeof(MARU_QueuedEvent) * capacity_power_of_2);
  if (!q->buffer) return false;

  memset(q->buffer, 0, sizeof(MARU_QueuedEvent) * capacity_power_of_2);
  return true;
}

void _maru_event_queue_cleanup(MARU_EventQueue *q, MARU_Context_Base *ctx) {
  if (q->buffer) {
    maru_context_free(ctx, q->buffer);
    q->buffer = NULL;
  }
}

void _maru_event_queue_update_metrics(MARU_EventQueue *q, MARU_UserEventMetrics *metrics) {
  if (!q || !metrics) return;
#ifdef MARU_GATHER_METRICS
  metrics->current_capacity = q->capacity;
  metrics->peak_count = (uint32_t)atomic_load_explicit(&q->high_watermark, memory_order_relaxed);
  metrics->drop_count = (uint32_t)atomic_load_explicit(&q->drop_count, memory_order_relaxed);
#else
  (void)q; (void)metrics;
#endif
}

bool _maru_event_queue_push(MARU_EventQueue *q, MARU_EventId type, 
                            MARU_Window *window, MARU_UserDefinedEvent evt) {
  size_t head, tail;
  
  do {
    head = atomic_load_explicit(&q->head, memory_order_relaxed);
    tail = atomic_load_explicit(&q->tail, memory_order_acquire);
    
    if (head - tail >= q->capacity) {
#ifdef MARU_GATHER_METRICS
      atomic_fetch_add_explicit(&q->drop_count, 1, memory_order_relaxed);
#endif
      return false; // Full
    }
  } while (!atomic_compare_exchange_weak_explicit(&q->head, &head, head + 1, 
                                                  memory_order_relaxed, memory_order_relaxed));

#ifdef MARU_GATHER_METRICS
  // Track High Watermark
  // This is a "relaxed" upper bound of usage. 
  // 'tail' might have moved forward since we loaded it, so actual usage might be lower.
  // This ensures we never underestimate the peak.
  size_t usage = (head + 1) - tail;
  size_t hwm = atomic_load_explicit(&q->high_watermark, memory_order_relaxed);
  while (usage > hwm) {
    if (atomic_compare_exchange_weak_explicit(&q->high_watermark, &hwm, usage, 
                                              memory_order_relaxed, memory_order_relaxed)) {
        break;
    }
    // If CAS failed, hwm was updated by another thread. 
    // Loop checks again if our usage is still higher than the new hwm.
  }
#endif

  // We reserved the slot at 'head'
  MARU_QueuedEvent *slot = &q->buffer[head & q->mask];

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
  
  // Publish
  atomic_store_explicit(&slot->state, 2, memory_order_release);
  return true;
}

bool _maru_event_queue_pop(MARU_EventQueue *q, MARU_EventId *out_type, 
                           MARU_Window **out_window, MARU_UserDefinedEvent *out_evt) {
  size_t tail = atomic_load_explicit(&q->tail, memory_order_relaxed);
  MARU_QueuedEvent *slot = &q->buffer[tail & q->mask];

  int state = atomic_load_explicit(&slot->state, memory_order_acquire);
  if (state != 2) {
    return false; // Empty or writing in progress
  }

  *out_type = slot->type;
  *out_window = slot->window;
  *out_evt = slot->evt;

  atomic_store_explicit(&slot->state, 0, memory_order_release);
  atomic_store_explicit(&q->tail, tail + 1, memory_order_release);
  
  return true;
}
