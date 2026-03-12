// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_EVENTS_HPP_INCLUDED
#define MARU_CPP_EVENTS_HPP_INCLUDED

#include "maru/maru.h"
#include "maru/cpp/fwd.hpp"

namespace maru {

/**
 * @brief A thin wrapper that bundles a specific event payload with its window metadata.
 * 
 * @note LIFETIME: Event objects and their underlying data are only valid 
 * for the duration of the callback.
 */
template <typename T>
struct Event {
    MARU_Window* window;
    const T& data;

    const T* operator->() const { return &data; }
    operator const T&() const { return data; }
};

template <typename T>
struct QueuedEvent {
    MARU_WindowId window_id;
    const T& data;

    const T* operator->() const { return &data; }
    operator const T&() const { return data; }
};

typedef Event<MARU_WindowReadyEvent> WindowReadyEvent;
typedef Event<MARU_CloseRequestedEvent> CloseRequestedEvent;
typedef Event<MARU_WindowResizedEvent> WindowResizedEvent;
typedef Event<MARU_WindowStateChangedEvent> WindowStateChangedEvent;
typedef Event<MARU_KeyChangedEvent> KeyChangedEvent;
typedef Event<MARU_MouseMovedEvent> MouseMovedEvent;
typedef Event<MARU_MouseButtonChangedEvent> MouseButtonChangedEvent;
typedef Event<MARU_MouseScrolledEvent> MouseScrolledEvent;
typedef Event<MARU_IdleChangedEvent> IdleEvent;
typedef Event<MARU_MonitorChangedEvent> MonitorChangedEvent;
typedef Event<MARU_MonitorModeChangedEvent> MonitorModeEvent;
typedef Event<MARU_DropEvent> DropEnteredEvent;
typedef Event<MARU_DropEvent> DropHoveredEvent;
typedef Event<MARU_DropExitedEvent> DropExitedEvent;
typedef Event<MARU_DropEvent> DropDroppedEvent;
typedef Event<MARU_DataReceivedEvent> DataReceivedEvent;
typedef Event<MARU_DataRequestEvent> DataRequestEvent;
typedef Event<MARU_DataConsumedEvent> DataConsumedEvent;
typedef Event<MARU_DragFinishedEvent> DragFinishedEvent;
typedef Event<MARU_ControllerChangedEvent> ControllerChangedEvent;
typedef Event<MARU_ControllerButtonChangedEvent> ControllerButtonChangedEvent;
typedef Event<MARU_WindowFrameEvent> WindowFrameEvent;
typedef Event<MARU_TextEditStartedEvent> TextEditStartedEvent;
typedef Event<MARU_TextEditUpdatedEvent> TextEditUpdatedEvent;
typedef Event<MARU_TextEditCommittedEvent> TextEditCommittedEvent;
typedef Event<MARU_TextEditEndedEvent> TextEditEndedEvent;

typedef QueuedEvent<MARU_WindowReadyEvent> QueuedWindowReadyEvent;
typedef QueuedEvent<MARU_CloseRequestedEvent> QueuedCloseRequestedEvent;
typedef QueuedEvent<MARU_WindowResizedEvent> QueuedWindowResizedEvent;
typedef QueuedEvent<MARU_WindowStateChangedEvent> QueuedWindowStateChangedEvent;
typedef QueuedEvent<MARU_KeyChangedEvent> QueuedKeyChangedEvent;
typedef QueuedEvent<MARU_MouseMovedEvent> QueuedMouseMovedEvent;
typedef QueuedEvent<MARU_MouseButtonChangedEvent> QueuedMouseButtonChangedEvent;
typedef QueuedEvent<MARU_MouseScrolledEvent> QueuedMouseScrolledEvent;
typedef QueuedEvent<MARU_IdleChangedEvent> QueuedIdleEvent;
typedef QueuedEvent<MARU_MonitorChangedEvent> QueuedMonitorChangedEvent;
typedef QueuedEvent<MARU_MonitorModeChangedEvent> QueuedMonitorModeEvent;
typedef QueuedEvent<MARU_DropEvent> QueuedDropEnteredEvent;
typedef QueuedEvent<MARU_DropEvent> QueuedDropHoveredEvent;
typedef QueuedEvent<MARU_DropExitedEvent> QueuedDropExitedEvent;
typedef QueuedEvent<MARU_DropEvent> QueuedDropDroppedEvent;
typedef QueuedEvent<MARU_DataReceivedEvent> QueuedDataReceivedEvent;
typedef QueuedEvent<MARU_DataRequestEvent> QueuedDataRequestEvent;
typedef QueuedEvent<MARU_DataConsumedEvent> QueuedDataConsumedEvent;
typedef QueuedEvent<MARU_DragFinishedEvent> QueuedDragFinishedEvent;
typedef QueuedEvent<MARU_ControllerChangedEvent> QueuedControllerChangedEvent;
typedef QueuedEvent<MARU_ControllerButtonChangedEvent> QueuedControllerButtonChangedEvent;
typedef QueuedEvent<MARU_WindowFrameEvent> QueuedWindowFrameEvent;
typedef QueuedEvent<MARU_TextEditStartedEvent> QueuedTextEditStartedEvent;
typedef QueuedEvent<MARU_TextEditUpdatedEvent> QueuedTextEditUpdatedEvent;
typedef QueuedEvent<MARU_TextEditCommittedEvent> QueuedTextEditCommittedEvent;
typedef QueuedEvent<MARU_TextEditEndedEvent> QueuedTextEditEndedEvent;

} // namespace maru

#endif // MARU_CPP_EVENTS_HPP_INCLUDED
