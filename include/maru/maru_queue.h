// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_QUEUE_H_INCLUDED
#define MARU_QUEUE_H_INCLUDED

#include "maru/c/core.h"
#include "maru/c/events.h"

/**
 * @file maru_queue.h
 * @brief Double-buffered event snapshot queue.
 *
 * A MARU_Queue provides an OPTIONAL way to consume events with a thread-safe
 * scanning phase, and event coalescence.
 * You don't have to use it, but if you are not bringing in your own event
 * queue, it makes dealing with events a lot smoother. It follows a push ->
 * commit -> scan lifecycle:
 *
 * 1. PUSH: On the primary thread, callback events are copied into the queue's
 *    internal active buffer.
 * 2. COMMIT: On the primary thread, the gathered events are made "stable",
 *    forming a consistent snapshot for readers.
 * 3. SCAN: From any thread, the stable snapshot can be iterated.
 *
 * Threading Rules:
 * - maru_pushQueue() and maru_commitQueue() MUST be called from the primary
 * thread.
 * - maru_scanQueue() can be called from any thread.
 * - External synchronization (e.g., an RW lock) MUST be used to ensure that
 *   maru_scanQueue() does not run concurrently with maru_commitQueue().
 *   Multiple concurrent calls to maru_scanQueue() are safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to an event queue. */
typedef struct MARU_Queue MARU_Queue;

/**
 * @brief Event mask of payloads safe to queue by value via maru_pushQueue().
 *
 * Any event outside this mask may carry transient pointers/handles whose
 * lifetime only spans the active pump callback.
 */
#define MARU_QUEUE_SAFE_EVENT_MASK                                              \
  (MARU_EVENT_MASK(MARU_EVENT_CLOSE_REQUESTED) |                               \
   MARU_EVENT_MASK(MARU_EVENT_WINDOW_RESIZED) |                                \
   MARU_EVENT_MASK(MARU_EVENT_KEY_STATE_CHANGED) |                             \
   MARU_EVENT_MASK(MARU_EVENT_WINDOW_READY) |                                  \
   MARU_EVENT_MASK(MARU_EVENT_MOUSE_MOVED) |                                   \
   MARU_EVENT_MASK(MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED) |                    \
   MARU_EVENT_MASK(MARU_EVENT_MOUSE_SCROLLED) |                                \
   MARU_EVENT_MASK(MARU_EVENT_IDLE_STATE_CHANGED) |                            \
   MARU_EVENT_MASK(MARU_EVENT_WINDOW_FRAME) |                                  \
   MARU_EVENT_MASK(MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED) |             \
   MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_START) |                               \
   MARU_EVENT_MASK(MARU_EVENT_TEXT_EDIT_END) |                                 \
   MARU_EVENT_MASK(MARU_EVENT_USER_0) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_1) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_2) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_3) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_4) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_5) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_6) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_7) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_8) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_9) |                                        \
   MARU_EVENT_MASK(MARU_EVENT_USER_10) |                                       \
   MARU_EVENT_MASK(MARU_EVENT_USER_11) |                                       \
   MARU_EVENT_MASK(MARU_EVENT_USER_12) |                                       \
   MARU_EVENT_MASK(MARU_EVENT_USER_13) |                                       \
   MARU_EVENT_MASK(MARU_EVENT_USER_14) |                                       \
   MARU_EVENT_MASK(MARU_EVENT_USER_15))

/** @brief Returns true when an event id is safe for maru_pushQueue(). */
static inline bool maru_isQueueSafeEventId(MARU_EventId type) {
  return maru_eventMaskHas(MARU_QUEUE_SAFE_EVENT_MASK, type);
}

/** @brief Cumulative queue overflow/compaction counters. */
typedef struct MARU_QueueMetrics {
  /** @brief Max active-buffer occupancy observed since last reset. */
  uint32_t peak_active_count;
  /** @brief Number of incoming events dropped because the queue stayed full. */
  uint32_t overflow_drop_count;
  /** @brief Number of overflow compaction sweeps executed. */
  uint32_t overflow_compact_count;
  /** @brief Number of stale events removed by overflow compaction. */
  uint32_t overflow_events_compacted;
  /**
   * @brief Drop counters indexed by `MARU_EventId` value.
   *
   * Entry `overflow_drop_count_by_event[id]` is incremented when an incoming
   * event of that type is dropped after overflow compaction.
   */
  uint32_t overflow_drop_count_by_event[MARU_EVENT_USER_15 + 1];
} MARU_QueueMetrics;

/**
 * @brief Creates a new event queue.
 *
 * @param ctx The library context this queue will be associated with.
 * @param capacity The internal buffer size
 * @param out_queue Pointer to receive the created queue handle.
 * @return MARU_SUCCESS on success, or an error code.
 */
MARU_API MARU_Status maru_createQueue(MARU_Context *ctx, uint32_t capacity,
                                      MARU_Queue **out_queue);

/**
 * @brief Destroys an event queue and frees its resources.
 *
 * @param queue The queue to destroy.
 */
MARU_API void maru_destroyQueue(MARU_Queue *queue);

/**
 * @brief Pushes one event into the queue's transient active buffer.
 *
 * Typical usage is from inside the callback provided to maru_pumpEvents().
 * Must be called on the primary thread.
 *
 * @param queue The event queue.
 * @param type Event id.
 * @param window Event window (may be NULL).
 * @param event Event payload to copy by value.
 * @return MARU_SUCCESS on success, or an error code.
 */
MARU_API MARU_Status maru_pushQueue(MARU_Queue *queue, MARU_EventId type,
                                    MARU_Window *window,
                                    const MARU_Event *event);

/**
 * @brief Makes all pushed events since the last commit available for scanning.
 *
 * This "freeze" the current set of events into a stable snapshot.
 * Must be called on the primary thread.
 *
 * @param queue The event queue.
 * @return MARU_SUCCESS on success, or an error code.
 */
MARU_API MARU_Status maru_commitQueue(MARU_Queue *queue);

/**
 * @brief Iterates over the stable snapshot of events.
 *
 * Can be called from any thread. Must be externally synchronized with
 * maru_commitQueue().
 *
 * @param queue The event queue.
 * @param mask Bitmask of event types to include in the scan.
 * @param callback Function to invoke for each matching event.
 * @param userdata User-provided pointer passed to the callback.
 */
MARU_API void maru_scanQueue(MARU_Queue *queue, MARU_EventMask mask,
                             MARU_EventCallback callback, void *userdata);

/**
 * @brief Sets a bitmask of event types that should be coalesced.
 *
 * When an event type is in the coalesce mask, and the latest event in the
 * active buffer is of the same type and for the same window, the new event
 * will be combined into the existing one rather than being appended.
 *
 * Coalescing logic:
 * - MARU_EVENT_MOUSE_MOVED: Updates position, accumulates delta and raw_delta.
 * - MARU_EVENT_MOUSE_SCROLLED: Accumulates delta and steps.
 * - MARU_EVENT_WINDOW_RESIZED: Updates geometry.
 *
 * Overflow behavior:
 * - When the queue is full, a compaction sweep is attempted before dropping.
 * - Compaction keeps the latest coalescible event per (type, window) and folds
 *   older duplicates into it using the same accumulation semantics.
 * - If the queue remains full after compaction, the incoming event is dropped.
 *
 * @param queue The event queue.
 * @param mask Bitmask of MARU_EventId bits to coalesce.
 */
MARU_API void maru_setQueueCoalesceMask(MARU_Queue *queue,
                                        MARU_EventMask mask);

/**
 * @brief Reads cumulative queue metrics for the queue.
 *
 * @param queue The queue to inspect (may be NULL).
 * @param out_metrics Receives metrics, zeroed when queue is NULL.
 */
MARU_API void maru_getQueueMetrics(const MARU_Queue *queue,
                                    MARU_QueueMetrics *out_metrics);

/**
 * @brief Resets queue metrics counters to zero.
 *
 * @param queue The queue to reset (may be NULL).
 */
MARU_API void maru_resetQueueMetrics(MARU_Queue *queue);

#ifdef __cplusplus
}
#endif

#endif // MARU_QUEUE_H_INCLUDED
