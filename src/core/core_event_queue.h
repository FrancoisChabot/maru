// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CORE_EVENT_QUEUE_H_INCLUDED
#define MARU_CORE_EVENT_QUEUE_H_INCLUDED

#include "maru/c/core.h"
#include "maru/c/events.h"

#ifdef __cplusplus
#include <atomic>
#define _MARU_ATOMIC(T) std::atomic<T>
#else
#include <stdatomic.h>
#include <stdalign.h>
#define _MARU_ATOMIC(T) _Atomic(T)
#endif

typedef struct MARU_QueuedEvent {
  _MARU_ATOMIC(int) state; // 0: Free, 1: Writing, 2: Ready
  MARU_EventId type;
  MARU_Window *window;
  MARU_Event evt;
} MARU_QueuedEvent;

typedef struct MARU_EventQueue {
  MARU_QueuedEvent *buffer;
  uint32_t capacity;
  uint32_t mask; // capacity - 1
  
  // We use separate cache lines to avoid false sharing
  alignas(64) _MARU_ATOMIC(size_t) head; // Producers write here
  alignas(64) _MARU_ATOMIC(size_t) tail; // Consumer reads here
#ifdef MARU_GATHER_METRICS
  alignas(64) _MARU_ATOMIC(size_t) high_watermark; // Track peak usage
  alignas(64) _MARU_ATOMIC(size_t) drop_count; // Track failed pushes
#endif
} MARU_EventQueue;

typedef struct MARU_Context_Base MARU_Context_Base;

#include "maru/c/metrics.h"

// Returns true on success
bool _maru_event_queue_init(MARU_EventQueue *q, MARU_Context_Base *ctx, uint32_t capacity_power_of_2);
void _maru_event_queue_cleanup(MARU_EventQueue *q, MARU_Context_Base *ctx);

// Updates the provided metrics struct with current atomic counters
void _maru_event_queue_update_metrics(MARU_EventQueue *q, MARU_UserEventMetrics *metrics);

// Thread-safe push. Returns false if queue is full.
bool _maru_event_queue_push(MARU_EventQueue *q, MARU_EventId type, 
                            MARU_Window *window, MARU_Event evt);

// Single-consumer pop. Returns false if empty.
bool _maru_event_queue_pop(MARU_EventQueue *q, MARU_EventId *out_type, 
                           MARU_Window **out_window, MARU_Event *out_evt);

#endif
