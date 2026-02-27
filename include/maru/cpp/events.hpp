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
typedef Event<MARU_WindowCloseEvent> WindowCloseEvent;
typedef Event<MARU_WindowResizedEvent> WindowResizedEvent;
typedef Event<MARU_WindowPresentationStateEvent> WindowPresentationStateEvent;
typedef Event<MARU_KeyboardEvent> KeyboardEvent;
typedef Event<MARU_MouseMotionEvent> MouseMotionEvent;
typedef Event<MARU_MouseButtonEvent> MouseButtonEvent;
typedef Event<MARU_MouseScrollEvent> MouseScrollEvent;
typedef Event<MARU_IdleEvent> IdleEvent;
typedef Event<MARU_MonitorConnectionEvent> MonitorConnectionEvent;
typedef Event<MARU_MonitorModeEvent> MonitorModeEvent;
typedef Event<MARU_DropEnterEvent> DropEnterEvent;
typedef Event<MARU_DropHoverEvent> DropHoverEvent;
typedef Event<MARU_DropLeaveEvent> DropLeaveEvent;
typedef Event<MARU_DropEvent> DropEvent;
typedef Event<MARU_DataReceivedEvent> DataReceivedEvent;
typedef Event<MARU_DataRequestEvent> DataRequestEvent;
typedef Event<MARU_DataConsumedEvent> DataConsumedEvent;
typedef Event<MARU_DragFinishedEvent> DragFinishedEvent;
typedef Event<MARU_ControllerConnectionEvent> ControllerConnectionEvent;
typedef Event<MARU_ControllerButtonStateChangedEvent> ControllerButtonStateChangedEvent;
typedef Event<MARU_WindowFrameEvent> WindowFrameEvent;
typedef Event<MARU_TextEditStartEvent> TextEditStartEvent;
typedef Event<MARU_TextEditUpdateEvent> TextEditUpdateEvent;
typedef Event<MARU_TextEditCommitEvent> TextEditCommitEvent;
typedef Event<MARU_TextEditEndEvent> TextEditEndEvent;

} // namespace maru

#endif // MARU_CPP_EVENTS_HPP_INCLUDED
