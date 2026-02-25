// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DATA_EXCHANGE_H_INCLUDED
#define MARU_DATA_EXCHANGE_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/geometry.h"
#include "maru/c/events.h"
#include "maru/c/inputs.h"

/**
 * @file data_exchange.h
 * @brief Clipboard and drag-and-drop data exchange APIs.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing an OS-level window. */
typedef struct MARU_Window MARU_Window;

/* ----- Event Masks ----- */

#define MARU_MASK_DROP_ENTERED  MARU_EVENT_MASK(MARU_EVENT_DROP_ENTERED)
#define MARU_MASK_DROP_HOVERED  MARU_EVENT_MASK(MARU_EVENT_DROP_HOVERED)
#define MARU_MASK_DROP_EXITED   MARU_EVENT_MASK(MARU_EVENT_DROP_EXITED)
#define MARU_MASK_DROP_DROPPED  MARU_EVENT_MASK(MARU_EVENT_DROP_DROPPED)
#define MARU_MASK_DATA_RECEIVED MARU_EVENT_MASK(MARU_EVENT_DATA_RECEIVED)
#define MARU_MASK_DATA_REQUESTED MARU_EVENT_MASK(MARU_EVENT_DATA_REQUESTED)
#define MARU_MASK_DATA_CONSUMED MARU_EVENT_MASK(MARU_EVENT_DATA_CONSUMED)
#define MARU_MASK_DRAG_FINISHED MARU_EVENT_MASK(MARU_EVENT_DRAG_FINISHED)

/* ----- Functions ----- */

/** @brief Announces available data formats to the OS.
 *  This initiates a "lazy" offer. When the OS or another app requests
 *  the data, the library will fire an `MARU_DATA_REQUESTED` event.
 *
 *  For MARU_DATA_EXCHANGE_TARGET_DRAG_DROP, the `allowed_actions` parameter
 *  specifies which operations the source permits. For other targets,
 *  this parameter is ignored.
 */
MARU_Status maru_announceData(MARU_Window *window,
                              MARU_DataExchangeTarget target,
                              const char **mime_types, uint32_t count,
                              MARU_DropActionMask allowed_actions);

/** @brief Provides the actual payload in response to an `MARU_DATA_REQUESTED`
 * event. */
MARU_Status maru_provideData(const MARU_DataRequestEvent *request_event,
                             const void *data, size_t size,
                             MARU_DataProvideFlags flags);

/** @brief Asynchronously requests an arbitrary data payload from the OS. */
MARU_Status maru_requestData(MARU_Window *window,
                             MARU_DataExchangeTarget target,
                             const char *mime_type, void *user_tag);

MARU_Status maru_dataexchange_enable(MARU_Context *context);

/** @brief Retrieves MIME types currently available in the system buffer.

The returned list is valid until the next call to maru_pumpEvents().
*/
MARU_Status maru_getAvailableMIMETypes(MARU_Window *window,
                                       MARU_DataExchangeTarget target,
                                       MARU_MIMETypeList *out_list);

#ifdef __cplusplus
}
#endif

#endif // MARU_DATA_EXCHANGE_H_INCLUDED
