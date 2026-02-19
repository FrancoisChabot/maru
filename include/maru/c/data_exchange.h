// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DATA_EXCHANGE_H_INCLUDED
#define MARU_DATA_EXCHANGE_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "maru/c/core.h"

/**
 * @file data_exchange.h
 * @brief Clipboard and drag-and-drop data exchange APIs.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque handle representing an OS-level window. */
typedef struct MARU_Window MARU_Window;
/** @brief Unified standard event payload union. */
struct MARU_Event;
struct MARU_DataRequestEvent;

/** @brief Supported Drag and Drop actions for OS feedback. */
typedef enum MARU_DropAction {
  MARU_DROP_ACTION_NONE = 0,
  MARU_DROP_ACTION_COPY = 1 << 0,
  MARU_DROP_ACTION_MOVE = 1 << 1,
  MARU_DROP_ACTION_LINK = 1 << 2,
} MARU_DropAction;

/** @brief Bitmask of allowed drop actions. */
typedef uint32_t MARU_DropActionMask;

/** @brief System-managed data exchange targets. */
typedef enum MARU_DataExchangeTarget {
  MARU_DATA_EXCHANGE_TARGET_CLIPBOARD = 0,
  MARU_DATA_EXCHANGE_TARGET_PRIMARY = 1,
  MARU_DATA_EXCHANGE_TARGET_DRAG_DROP = 2,
} MARU_DataExchangeTarget;

/** @brief Announces available data formats to the OS. 
 *  This initiates a "lazy" offer. When the OS or another app requests 
 *  the data, the library will fire an `MARU_DATA_REQUESTED` event. 
 * 
 *  For MARU_DATA_EXCHANGE_TARGET_DRAG_DROP, the `allowed_actions` parameter 
 *  specifies which operations the source permits. For other targets, 
 *  this parameter is ignored.
 */
MARU_Status maru_announceData(MARU_Window *window, MARU_DataExchangeTarget target,
                                         const char **mime_types, uint32_t count,
                                         MARU_DropActionMask allowed_actions);

/** @brief Flags that influence how data is provided for a request. */
typedef enum MARU_DataProvideFlags {
  MARU_DATA_PROVIDE_FLAG_NONE = 0,
  /** Request zero-copy delivery */
  MARU_DATA_PROVIDE_FLAG_ZERO_COPY = 1 << 0,
} MARU_DataProvideFlags;

/** @brief Provides the actual payload in response to an `MARU_DATA_REQUESTED` event. */
MARU_Status maru_provideData(const struct MARU_DataRequestEvent *request_event,
                                       const void *data, size_t size, MARU_DataProvideFlags flags);

/** @brief Asynchronously requests an arbitrary data payload from the OS. */
MARU_Status maru_requestData(MARU_Window *window, MARU_DataExchangeTarget target,
                                        const char *mime_type, void *user_tag);

/** @brief A transient list of MIME type strings.
 *
 * The `mime_types` pointer is owned by the library and is only valid until the next
 * call to maru_pumpEvents().
 */
typedef struct MARU_MIMETypeList {
  const char *const *mime_types;
  uint32_t count;
} MARU_MIMETypeList;

/** @brief Retrieves MIME types currently available in the system buffer. 

The returned list is valid until the next call to maru_pumpEvents().
*/
MARU_Status maru_getAvailableMIMETypes(MARU_Window *window,
                                                MARU_DataExchangeTarget target,
                                                MARU_MIMETypeList *out_list);

#ifdef __cplusplus
}
#endif

#endif  // MARU_DATA_EXCHANGE_H_INCLUDED
