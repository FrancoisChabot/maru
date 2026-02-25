// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_DATA_EXCHANGE_H_INCLUDED
#define MARU_DATA_EXCHANGE_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "maru/c/core.h"
#include "maru/c/geometry.h"
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

/** @brief A transient list of MIME type strings.
 *
 * The `mime_types` pointer is owned by the library and is only valid until the
 * next call to maru_pumpEvents().
 */
typedef struct MARU_MIMETypeList {
  const char *const *mime_types;
  uint32_t count;
} MARU_MIMETypeList;

/** @brief Flags that influence how data is provided for a request. */
typedef enum MARU_DataProvideFlags {
  MARU_DATA_PROVIDE_FLAG_NONE = 0,
  /** Request zero-copy delivery */
  MARU_DATA_PROVIDE_FLAG_ZERO_COPY = 1 << 0,
} MARU_DataProvideFlags;

/* ----- Event Payloads ----- */

/** @brief Controls application response to a drag-and-drop hover. */
typedef struct MARU_DropFeedback {
  MARU_DropAction *action;
  void **session_userdata;
} MARU_DropFeedback;

/** @brief Payload for MARU_EVENT_DROP_ENTERED. */
typedef struct MARU_DropEnterEvent {
  MARU_Vec2Dip position;
  MARU_DropFeedback *feedback;
  const char **paths; // Can be NULL
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint16_t path_count;
} MARU_DropEnterEvent;

/** @brief Payload for MARU_EVENT_DROP_HOVERED. */
typedef struct MARU_DropHoverEvent {
  MARU_Vec2Dip position;
  MARU_DropFeedback *feedback;
  const char **paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint16_t path_count;
} MARU_DropHoverEvent;

/** @brief Payload for MARU_EVENT_DROP_EXITED. */
typedef struct MARU_DropLeaveEvent {
  void **session_userdata;
} MARU_DropLeaveEvent;

/** @brief Payload for MARU_EVENT_DROP_DROPPED. */
typedef struct MARU_DropEvent {
  MARU_Vec2Dip position;
  void **session_userdata;
  const char **paths;
  MARU_MIMETypeList available_types;
  MARU_ModifierFlags modifiers;
  uint16_t path_count;
} MARU_DropEvent;

/** @brief Payload for MARU_EVENT_DATA_RECEIVED. */
typedef struct MARU_DataReceivedEvent {
  void *user_tag;
  MARU_Status status;
  MARU_DataExchangeTarget target;
  const char *mime_type;
  const void *data;
  size_t size;
} MARU_DataReceivedEvent;

/** @brief Payload for MARU_EVENT_DATA_REQUESTED. */
typedef struct MARU_DataRequestEvent {
  MARU_DataExchangeTarget target;
  const char *mime_type;
  void *internal_handle;
} MARU_DataRequestEvent;

/** @brief Payload for MARU_EVENT_DATA_CONSUMED. */
typedef struct MARU_DataConsumedEvent {
  MARU_DataExchangeTarget target;
  const char *mime_type;
  const void *data;
  size_t size;
  MARU_DropAction action;
} MARU_DataConsumedEvent;

/** @brief Payload for MARU_EVENT_DRAG_FINISHED. */
typedef struct MARU_DragFinishedEvent {
  MARU_DropAction action;
} MARU_DragFinishedEvent;

/* ----- Event Masks ----- */

#define MARU_MASK_DROP_ENTERED  MARU_EVENT_MASK(MARU_EVENT_DROP_ENTERED)
#define MARU_MASK_DROP_HOVERED  MARU_EVENT_MASK(MARU_EVENT_DROP_HOVERED)
#define MARU_MASK_DROP_EXITED   MARU_EVENT_MASK(MARU_EVENT_DROP_EXITED)
#define MARU_MASK_DROP_DROPPED  MARU_EVENT_MASK(MARU_EVENT_DROP_DROPPED)
#define MARU_MASK_DATA_RECEIVED MARU_EVENT_MASK(MARU_EVENT_DATA_RECEIVED)
#define MARU_MASK_DATA_REQUESTED MARU_EVENT_MASK(MARU_EVENT_DATA_REQUESTED)
#define MARU_MASK_DATA_CONSUMED MARU_EVENT_MASK(MARU_EVENT_DATA_CONSUMED)
#define MARU_MASK_DRAG_FINISHED MARU_EVENT_MASK(MARU_EVENT_DRAG_FINISHED)

/* ----- Accessors ----- */

static inline const MARU_DropEnterEvent* maru_asDropEnter(const MARU_Event* ev) {
  return (const MARU_DropEnterEvent*)&ev->ext;
}

static inline const MARU_DropHoverEvent* maru_asDropHover(const MARU_Event* ev) {
  return (const MARU_DropHoverEvent*)&ev->ext;
}

static inline const MARU_DropLeaveEvent* maru_asDropLeave(const MARU_Event* ev) {
  return (const MARU_DropLeaveEvent*)&ev->ext;
}

static inline const MARU_DropEvent* maru_asDrop(const MARU_Event* ev) {
  return (const MARU_DropEvent*)&ev->ext;
}

static inline const MARU_DataReceivedEvent* maru_asDataReceived(const MARU_Event* ev) {
  return (const MARU_DataReceivedEvent*)&ev->ext;
}

static inline const MARU_DataRequestEvent* maru_asDataRequested(const MARU_Event* ev) {
  return (const MARU_DataRequestEvent*)&ev->ext;
}

static inline const MARU_DataConsumedEvent* maru_asDataConsumed(const MARU_Event* ev) {
  return (const MARU_DataConsumedEvent*)&ev->ext;
}

static inline const MARU_DragFinishedEvent* maru_asDragFinished(const MARU_Event* ev) {
  return (const MARU_DragFinishedEvent*)&ev->ext;
}

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
