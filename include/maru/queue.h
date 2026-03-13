// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_QUEUE_H_INCLUDED
#define MARU_QUEUE_H_INCLUDED

#include "maru/maru.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_Queue MARU_Queue;

/*
 * Queue replay callback. Unlike live pump callbacks, queue scans report the
 * stable target window id that was captured at enqueue time rather than a live
 * MARU_Window* handle.
 */
typedef void (*MARU_QueueEventCallback)(MARU_EventId type,
                                        MARU_WindowId window_id,
                                        const MARU_Event* evt,
                                        void* userdata);

/*
 * Events excluded from this mask carry transient handles or borrowed pointer
 * payloads that are not safe to snapshot into a MARU_Queue without additional
 * copying or retention by the application.
 *
 * Queue-safe replay preserves only the event payload and the target
 * MARU_WindowId. It does not preserve a live MARU_Window* handle.
 */
#define MARU_QUEUE_SAFE_EVENT_MASK                                                              \
  (MARU_EVENT_MASK(MARU_EVENT_CLOSE_REQUESTED) | MARU_EVENT_MASK(MARU_EVENT_WINDOW_RESIZED) |   \
   MARU_EVENT_MASK(MARU_EVENT_KEY_CHANGED) | MARU_EVENT_MASK(MARU_EVENT_WINDOW_READY) |         \
   MARU_EVENT_MASK(MARU_EVENT_MOUSE_MOVED) | MARU_EVENT_MASK(MARU_EVENT_MOUSE_BUTTON_CHANGED) | \
   MARU_EVENT_MASK(MARU_EVENT_MOUSE_SCROLLED) | MARU_EVENT_MASK(MARU_EVENT_IDLE_CHANGED) |      \
   MARU_EVENT_MASK(MARU_EVENT_WINDOW_FRAME) | MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_STARTED) |   \
   MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_ENDED) |                                                 \
   MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_NAVIGATION) |                                            \
   MARU_EVENT_MASK(MARU_EVENT_DRAG_FINISHED) | MARU_EVENT_MASK(MARU_EVENT_USER_0) |             \
   MARU_EVENT_MASK(MARU_EVENT_USER_1) | MARU_EVENT_MASK(MARU_EVENT_USER_2) |                    \
   MARU_EVENT_MASK(MARU_EVENT_USER_3) | MARU_EVENT_MASK(MARU_EVENT_USER_4) |                    \
   MARU_EVENT_MASK(MARU_EVENT_USER_5) | MARU_EVENT_MASK(MARU_EVENT_USER_6) |                    \
   MARU_EVENT_MASK(MARU_EVENT_USER_7) | MARU_EVENT_MASK(MARU_EVENT_USER_8) |                    \
   MARU_EVENT_MASK(MARU_EVENT_USER_9) | MARU_EVENT_MASK(MARU_EVENT_USER_10) |                   \
   MARU_EVENT_MASK(MARU_EVENT_USER_11) | MARU_EVENT_MASK(MARU_EVENT_USER_12) |                  \
   MARU_EVENT_MASK(MARU_EVENT_USER_13) | MARU_EVENT_MASK(MARU_EVENT_USER_14) |                  \
   MARU_EVENT_MASK(MARU_EVENT_USER_15))

static inline bool maru_isQueueSafeEventId(MARU_EventId type) {
  return maru_eventMaskHas(MARU_QUEUE_SAFE_EVENT_MASK, type);
}

typedef struct MARU_QueueCreateInfo {
  MARU_Allocator allocator;
  MARU_DiagnosticCallback diagnostic_cb;
  void* diagnostic_userdata;
  uint32_t capacity;
} MARU_QueueCreateInfo;

#define MARU_QUEUE_CREATE_INFO_DEFAULT                                                              \
  {                                                                                                 \
      .allocator = {.alloc_cb = NULL, .realloc_cb = NULL, .free_cb = NULL, .userdata = NULL},     \
      .diagnostic_cb = NULL,                                                                        \
      .diagnostic_userdata = NULL,                                                                  \
      .capacity = 256u,                                                                             \
  }

/*
 * Standalone event snapshot/coalescing helper.
 *
 * A MARU_Queue is not attached to a MARU_Context and is never destroyed
 * implicitly by maru_destroyContext(). Queue APIs return queue-local success or
 * failure only; MARU_CONTEXT_LOST is not meaningful here.
 *
 * Threading contract:
 * - maru_createQueue() establishes a fixed owner thread for the queue.
 * - maru_destroyQueue(), maru_pushQueue(), maru_commitQueue(),
 *   maru_scanQueue() and maru_setQueueCoalesceMask() are creator-thread APIs.
 *   A queue is a single-owner helper; cross-thread scanning or mutation is not
 *   part of the public contract.
 * - Only queue-safe event ids may be pushed. Use MARU_QUEUE_SAFE_EVENT_MASK or
 *   maru_isQueueSafeEventId() when capturing events from maru_pumpEvents().
 * - Window-targeted events are keyed by the stable MARU_WindowId captured at
 *   enqueue time. Use MARU_WINDOW_ID_NONE for context-scoped events.
 *
 * Coalescing contract:
 * - maru_setQueueCoalesceMask() marks event ids as eligible for coalescing in
 *   the queue's active buffer.
 * - Only events with the same `(type, window_id)` pair may coalesce.
 * - Adjacent eligible pushes coalesce immediately instead of appending a new
 *   active entry.
 * - If the active buffer fills, the queue may compact it by folding older
 *   eligible entries into the latest surviving entry with the same
 *   `(type, window_id)` pair while preserving the order of surviving entries.
 * - `MARU_EVENT_MOUSE_MOVED` and `MARU_EVENT_MOUSE_SCROLLED` accumulate their
 *   deltas/steps into the latest survivor.
 * - `MARU_EVENT_WINDOW_RESIZED` keeps the latest geometry.
 * - Other eligible event ids keep the latest payload.
 * - Mask bits for event ids that are never pushed into the queue have no
 *   observable effect.
 *
 * Diagnostic model:
 * - Queue APIs intentionally use `bool` rather than `MARU_Status`.
 * - Because a queue is not bound to a context, there is no queue-specific
 *   equivalent of `MARU_CONTEXT_LOST`.
 * - `MARU_QueueCreateInfo.diagnostic_cb`, if non-null, receives best-effort
 *   queue-local diagnostics for runtime failures. Queue diagnostics always
 *   report `info->context == NULL` and `info->subject_kind ==
 *   MARU_DIAGNOSTIC_SUBJECT_NONE`.
 * - A `false` result means the requested queue operation failed for queue-local
 *   reasons such as invalid configuration, allocation failure, or queue
 *   overflow/compaction failure.
 * - The diagnostic callback runs synchronously on the queue owner thread and
 *   must not re-enter the same queue.
 */
MARU_API bool maru_createQueue(const MARU_QueueCreateInfo* create_info,
                               MARU_Queue** out_queue);
MARU_API void maru_destroyQueue(MARU_Queue* queue);
MARU_API bool maru_pushQueue(MARU_Queue* queue,
                             MARU_EventId type,
                             MARU_WindowId window_id,
                             const MARU_Event* event);
MARU_API bool maru_commitQueue(MARU_Queue* queue);
MARU_API void maru_scanQueue(MARU_Queue* queue,
                             MARU_EventMask mask,
                             MARU_QueueEventCallback callback,
                             void* userdata);
MARU_API void maru_setQueueCoalesceMask(MARU_Queue* queue, MARU_EventMask mask);

#ifdef __cplusplus
}
#endif

#endif
