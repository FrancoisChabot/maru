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
typedef Event<MARU_DropEnteredEvent> DropEnteredEvent;
typedef Event<MARU_DropHoveredEvent> DropHoveredEvent;
typedef Event<MARU_DropExitedEvent> DropExitedEvent;
typedef Event<MARU_DropDroppedEvent> DropDroppedEvent;
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

} // namespace maru

#endif // MARU_CPP_EVENTS_HPP_INCLUDED
