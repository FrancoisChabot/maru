// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_QUEUE_H_INCLUDED
#define MARU_QUEUE_H_INCLUDED

#include "maru/c/core.h"
#include "maru/c/events.h"

/**
 * @file maru_queue.h
 * @brief User-facing SPMC event queue.
 *
 * A MARU_Queue provides an optional, thread-safe way to consume events.
 * It follows a pump -> commit -> scan lifecycle:
 *
 * 1. PUMP: On the primary thread, events are gathered from the OS and pushed
 *    into the queue's internal buffer.
 * 2. COMMIT: On the primary thread, the gathered events are made "stable",
 *    forming a consistent snapshot for readers.
 * 3. SCAN: From any thread, the stable snapshot can be iterated.
 *
 * Threading Rules:
 * - maru_queue_pump() and maru_queue_commit() MUST be called from the primary thread.
 * - maru_queue_scan() can be called from any thread.
 * - External synchronization (e.g., an RW lock) MUST be used to ensure that
 *   maru_queue_scan() does not run concurrently with maru_queue_commit().
 *   Multiple concurrent calls to maru_queue_scan() are safe.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle to an event queue. */
typedef struct MARU_Queue MARU_Queue;

/**
 * @brief Creates a new event queue.
 *
 * @param ctx The library context this queue will be associated with.
 * @param capacity_power_of_2 The internal buffer size (must be a power of 2).
 * @param out_queue Pointer to receive the created queue handle.
 * @return MARU_SUCCESS on success, or an error code.
 */
MARU_API MARU_Status maru_queue_create(MARU_Context *ctx, uint32_t capacity_power_of_2,
                                       MARU_Queue **out_queue);

/**
 * @brief Destroys an event queue and frees its resources.
 *
 * @param queue The queue to destroy.
 */
MARU_API void maru_queue_destroy(MARU_Queue *queue);

/**
 * @brief Pumps events from the OS into the queue's transient buffer.
 *
 * This function invokes the underlying context's event pumping mechanism.
 * Must be called on the primary thread.
 *
 * @param queue The event queue.
 * @param timeout_ms How long to wait for events from the OS.
 * @return MARU_SUCCESS on success, or an error code.
 */
MARU_API MARU_Status maru_queue_pump(MARU_Queue *queue, uint32_t timeout_ms);

/**
 * @brief Makes all pumped events since the last commit available for scanning.
 *
 * This "freezes" the current set of events into a stable snapshot.
 * Must be called on the primary thread.
 *
 * @param queue The event queue.
 * @return MARU_SUCCESS on success, or an error code.
 */
MARU_API MARU_Status maru_queue_commit(MARU_Queue *queue);

/**
 * @brief Iterates over the stable snapshot of events.
 *
 * Can be called from any thread. Must be externally synchronized with
 * maru_queue_commit().
 *
 * @param queue The event queue.
 * @param mask Bitmask of event types to include in the scan.
 * @param callback Function to invoke for each matching event.
 * @param userdata User-provided pointer passed to the callback.
 */
MARU_API void maru_queue_scan(MARU_Queue *queue, MARU_EventMask mask,
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
 * @param queue The event queue.
 * @param mask Bitmask of MARU_EventId bits to coalesce.
 */
MARU_API void maru_queue_set_coalesce_mask(MARU_Queue *queue, MARU_EventMask mask);

#ifdef __cplusplus
}
#endif

#endif  // MARU_QUEUE_H_INCLUDED
