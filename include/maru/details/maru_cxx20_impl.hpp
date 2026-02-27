// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CXX20_IMPL_HPP_INCLUDED
#define MARU_CXX20_IMPL_HPP_INCLUDED

#include <utility>

namespace maru {

namespace details {

template <typename Visitor>
void dispatchVisitor(MARU_EventId type, MARU_Window *window, const MARU_Event &evt, Visitor &&f) {
  using F_raw = std::remove_cvref_t<Visitor>;
  switch (type) {
    case MARU_EVENT_WINDOW_READY:
      if constexpr (std::invocable<F_raw, WindowReadyEvent>)
        f(WindowReadyEvent{window, evt.window_ready});
      break;
    case MARU_EVENT_CLOSE_REQUESTED:
      if constexpr (std::invocable<F_raw, WindowCloseEvent>)
        f(WindowCloseEvent{window, evt.close_requested});
      break;
    case MARU_EVENT_WINDOW_RESIZED:
      if constexpr (std::invocable<F_raw, WindowResizedEvent>)
        f(WindowResizedEvent{window, evt.resized});
      break;
    case MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED:
      if constexpr (std::invocable<F_raw, WindowPresentationStateEvent>)
        f(WindowPresentationStateEvent{window, evt.presentation});
      break;
    case MARU_EVENT_KEY_STATE_CHANGED:
      if constexpr (std::invocable<F_raw, KeyboardEvent>) 
        f(KeyboardEvent{window, evt.key});
      break;
    case MARU_EVENT_MOUSE_MOVED:
      if constexpr (std::invocable<F_raw, MouseMotionEvent>)
        f(MouseMotionEvent{window, evt.mouse_motion});
      break;
    case MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED:
      if constexpr (std::invocable<F_raw, MouseButtonEvent>)
        f(MouseButtonEvent{window, evt.mouse_button});
      break;
    case MARU_EVENT_MOUSE_SCROLLED:
      if constexpr (std::invocable<F_raw, MouseScrollEvent>)
        f(MouseScrollEvent{window, evt.mouse_scroll});
      break;
    case MARU_EVENT_IDLE_STATE_CHANGED:
      if constexpr (std::invocable<F_raw, IdleEvent>) 
        f(IdleEvent{window, evt.idle});
      break;
    case MARU_EVENT_MONITOR_CONNECTION_CHANGED:
      if constexpr (std::invocable<F_raw, MonitorConnectionEvent>)
        f(MonitorConnectionEvent{window, evt.monitor_connection});
      break;
    case MARU_EVENT_MONITOR_MODE_CHANGED:
      if constexpr (std::invocable<F_raw, MonitorModeEvent>)
        f(MonitorModeEvent{window, evt.monitor_mode});
      break;
    case MARU_EVENT_DROP_ENTERED:
      if constexpr (std::invocable<F_raw, DropEnterEvent>)
        f(DropEnterEvent{window, evt.drop_enter});
      break;
    case MARU_EVENT_DROP_HOVERED:
      if constexpr (std::invocable<F_raw, DropHoverEvent>)
        f(DropHoverEvent{window, evt.drop_hover});
      break;
    case MARU_EVENT_DROP_EXITED:
      if constexpr (std::invocable<F_raw, DropLeaveEvent>)
        f(DropLeaveEvent{window, evt.drop_leave});
      break;
    case MARU_EVENT_DROP_DROPPED:
      if constexpr (std::invocable<F_raw, DropEvent>)
        f(DropEvent{window, evt.drop});
      break;
    case MARU_EVENT_DATA_RECEIVED:
      if constexpr (std::invocable<F_raw, DataReceivedEvent>)
        f(DataReceivedEvent{window, evt.data_received});
      break;
    case MARU_EVENT_DATA_REQUESTED:
      if constexpr (std::invocable<F_raw, DataRequestEvent>)
        f(DataRequestEvent{window, evt.data_requested});
      break;
    case MARU_EVENT_DATA_CONSUMED:
      if constexpr (std::invocable<F_raw, DataConsumedEvent>)
        f(DataConsumedEvent{window, evt.data_consumed});
      break;
    case MARU_EVENT_DRAG_FINISHED:
      if constexpr (std::invocable<F_raw, DragFinishedEvent>)
        f(DragFinishedEvent{window, evt.drag_finished});
      break;
    case MARU_EVENT_CONTROLLER_CONNECTION_CHANGED:
      if constexpr (std::invocable<F_raw, ControllerConnectionEvent>)
        f(ControllerConnectionEvent{window, evt.controller_connection});
      break;
    case MARU_EVENT_CONTROLLER_BUTTON_STATE_CHANGED:
      if constexpr (std::invocable<F_raw, ControllerButtonStateChangedEvent>)
        f(ControllerButtonStateChangedEvent{window, evt.controller_button_state_changed});
      break;
    case MARU_EVENT_WINDOW_FRAME:
      if constexpr (std::invocable<F_raw, WindowFrameEvent>)
        f(WindowFrameEvent{window, evt.frame});
      break;
    case MARU_EVENT_TEXT_EDIT_START:
      if constexpr (std::invocable<F_raw, TextEditStartEvent>)
        f(TextEditStartEvent{window, evt.text_edit_start});
      break;
    case MARU_EVENT_TEXT_EDIT_UPDATE:
      if constexpr (std::invocable<F_raw, TextEditUpdateEvent>)
        f(TextEditUpdateEvent{window, evt.text_edit_update});
      break;
    case MARU_EVENT_TEXT_EDIT_COMMIT:
      if constexpr (std::invocable<F_raw, TextEditCommitEvent>)
        f(TextEditCommitEvent{window, evt.text_edit_commit});
      break;
    case MARU_EVENT_TEXT_EDIT_END:
      if constexpr (std::invocable<F_raw, TextEditEndEvent>)
        f(TextEditEndEvent{window, evt.text_edit_end});
      break;
    default:
      break;
  }
}

}  // namespace details

template <typename Visitor>
void EventDispatcher<Visitor>::callback(MARU_EventId type, MARU_Window *window,
                                       const MARU_Event *event, void *userdata) {
  auto *self = static_cast<EventDispatcher<Visitor> *>(userdata);
  if constexpr (PartialEventVisitor<Visitor>) {
    details::dispatchVisitor(type, window, *event, self->m_visitor);
  } else if constexpr (std::invocable<Visitor, MARU_EventId, MARU_Window *, const MARU_Event &>) {
    self->m_visitor(type, window, *event);
  }
}

}  // namespace maru

#endif  // MARU_CXX20_IMPL_HPP_INCLUDED
