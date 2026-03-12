// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CXX20_IMPL_HPP_INCLUDED
#define MARU_CXX20_IMPL_HPP_INCLUDED

#include <utility>

namespace maru {

namespace details {

template <typename F>
consteval MARU_EventMask generateMask() {
  MARU_EventMask mask = 0;
  if constexpr (std::invocable<F, WindowReadyEvent>) mask |= MARU_MASK_WINDOW_READY;
  if constexpr (std::invocable<F, CloseRequestedEvent>) mask |= MARU_MASK_CLOSE_REQUESTED;
  if constexpr (std::invocable<F, WindowResizedEvent>) mask |= MARU_MASK_WINDOW_RESIZED;
  if constexpr (std::invocable<F, WindowStateChangedEvent>) mask |= MARU_MASK_WINDOW_STATE_CHANGED;
  if constexpr (std::invocable<F, KeyChangedEvent>) mask |= MARU_MASK_KEY_CHANGED;
  if constexpr (std::invocable<F, MouseMovedEvent>) mask |= MARU_MASK_MOUSE_MOVED;
  if constexpr (std::invocable<F, MouseButtonChangedEvent>) mask |= MARU_MASK_MOUSE_BUTTON_CHANGED;
  if constexpr (std::invocable<F, MouseScrolledEvent>) mask |= MARU_MASK_MOUSE_SCROLLED;
  if constexpr (std::invocable<F, IdleEvent>) mask |= MARU_MASK_IDLE_CHANGED;
  if constexpr (std::invocable<F, MonitorChangedEvent>) mask |= MARU_MASK_MONITOR_CHANGED;
  if constexpr (std::invocable<F, MonitorModeEvent>) mask |= MARU_MASK_MONITOR_MODE_CHANGED;
  if constexpr (std::invocable<F, DropEnteredEvent>) mask |= MARU_MASK_DROP_ENTERED;
  if constexpr (std::invocable<F, DropHoveredEvent>) mask |= MARU_MASK_DROP_HOVERED;
  if constexpr (std::invocable<F, DropExitedEvent>) mask |= MARU_MASK_DROP_EXITED;
  if constexpr (std::invocable<F, DropDroppedEvent>) mask |= MARU_MASK_DROP_DROPPED;
  if constexpr (std::invocable<F, DataReceivedEvent>) mask |= MARU_EVENT_MASK(MARU_EVENT_DATA_RECEIVED);
  if constexpr (std::invocable<F, DataRequestEvent>) mask |= MARU_EVENT_MASK(MARU_EVENT_DATA_REQUESTED);
  if constexpr (std::invocable<F, DataConsumedEvent>) mask |= MARU_EVENT_MASK(MARU_EVENT_DATA_CONSUMED);
  if constexpr (std::invocable<F, DragFinishedEvent>) mask |= MARU_EVENT_MASK(MARU_EVENT_DRAG_FINISHED);
  if constexpr (std::invocable<F, ControllerChangedEvent>) mask |= MARU_MASK_CONTROLLER_CHANGED;
  if constexpr (std::invocable<F, ControllerButtonChangedEvent>) mask |= MARU_EVENT_MASK(MARU_EVENT_CONTROLLER_BUTTON_CHANGED);
  if constexpr (std::invocable<F, WindowFrameEvent>) mask |= MARU_MASK_WINDOW_FRAME;
  if constexpr (std::invocable<F, TextEditStartedEvent>) mask |= MARU_MASK_TEXT_EDIT_STARTED;
  if constexpr (std::invocable<F, TextEditUpdatedEvent>) mask |= MARU_MASK_TEXT_EDIT_UPDATED;
  if constexpr (std::invocable<F, TextEditCommittedEvent>) mask |= MARU_MASK_TEXT_EDIT_COMMITTED;
  if constexpr (std::invocable<F, TextEditEndedEvent>) mask |= MARU_MASK_TEXT_EDIT_ENDED;
  return mask;
}

template <typename Visitor>
void dispatchVisitor(MARU_EventId type, MARU_Window *window, const MARU_Event &evt, Visitor &&f) {
  using F_raw = std::remove_cvref_t<Visitor>;
  switch (type) {
    case MARU_EVENT_WINDOW_READY:
      if constexpr (std::invocable<F_raw, WindowReadyEvent>)
        f(WindowReadyEvent{window, evt.window_ready});
      break;
    case MARU_EVENT_CLOSE_REQUESTED:
      if constexpr (std::invocable<F_raw, CloseRequestedEvent>)
        f(CloseRequestedEvent{window, evt.close_requested});
      break;
    case MARU_EVENT_WINDOW_RESIZED:
      if constexpr (std::invocable<F_raw, WindowResizedEvent>)
        f(WindowResizedEvent{window, evt.resized});
      break;
    case MARU_EVENT_WINDOW_STATE_CHANGED:
      if constexpr (std::invocable<F_raw, WindowStateChangedEvent>)
        f(WindowStateChangedEvent{window, evt.state_changed});
      break;
    case MARU_EVENT_KEY_CHANGED:
      if constexpr (std::invocable<F_raw, KeyChangedEvent>) 
        f(KeyChangedEvent{window, evt.key_changed});
      break;
    case MARU_EVENT_MOUSE_MOVED:
      if constexpr (std::invocable<F_raw, MouseMovedEvent>)
        f(MouseMovedEvent{window, evt.mouse_moved});
      break;
    case MARU_EVENT_MOUSE_BUTTON_CHANGED:
      if constexpr (std::invocable<F_raw, MouseButtonChangedEvent>)
        f(MouseButtonChangedEvent{window, evt.mouse_button_changed});
      break;
    case MARU_EVENT_MOUSE_SCROLLED:
      if constexpr (std::invocable<F_raw, MouseScrolledEvent>)
        f(MouseScrolledEvent{window, evt.mouse_scrolled});
      break;
    case MARU_EVENT_IDLE_CHANGED:
      if constexpr (std::invocable<F_raw, IdleEvent>) 
        f(IdleEvent{window, evt.idle_changed});
      break;
    case MARU_EVENT_MONITOR_CHANGED:
      if constexpr (std::invocable<F_raw, MonitorChangedEvent>)
        f(MonitorChangedEvent{window, evt.monitor_changed});
      break;
    case MARU_EVENT_MONITOR_MODE_CHANGED:
      if constexpr (std::invocable<F_raw, MonitorModeEvent>)
        f(MonitorModeEvent{window, evt.monitor_mode_changed});
      break;
    case MARU_EVENT_DROP_ENTERED:
      if constexpr (std::invocable<F_raw, DropEnteredEvent>)
        f(DropEnteredEvent{window, evt.drop_entered});
      break;
    case MARU_EVENT_DROP_HOVERED:
      if constexpr (std::invocable<F_raw, DropHoveredEvent>)
        f(DropHoveredEvent{window, evt.drop_hovered});
      break;
    case MARU_EVENT_DROP_EXITED:
      if constexpr (std::invocable<F_raw, DropExitedEvent>)
        f(DropExitedEvent{window, evt.drop_exited});
      break;
    case MARU_EVENT_DROP_DROPPED:
      if constexpr (std::invocable<F_raw, DropDroppedEvent>)
        f(DropDroppedEvent{window, evt.drop_dropped});
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
    case MARU_EVENT_CONTROLLER_CHANGED:
      if constexpr (std::invocable<F_raw, ControllerChangedEvent>)
        f(ControllerChangedEvent{window, evt.controller_changed});
      break;
    case MARU_EVENT_CONTROLLER_BUTTON_CHANGED:
      if constexpr (std::invocable<F_raw, ControllerButtonChangedEvent>)
        f(ControllerButtonChangedEvent{window, evt.controller_button_changed});
      break;
    case MARU_EVENT_WINDOW_FRAME:
      if constexpr (std::invocable<F_raw, WindowFrameEvent>)
        f(WindowFrameEvent{window, evt.frame});
      break;
    case MARU_EVENT_TEXT_EDIT_STARTED:
      if constexpr (std::invocable<F_raw, TextEditStartedEvent>)
        f(TextEditStartedEvent{window, evt.text_edit_started});
      break;
    case MARU_EVENT_TEXT_EDIT_UPDATED:
      if constexpr (std::invocable<F_raw, TextEditUpdatedEvent>)
        f(TextEditUpdatedEvent{window, evt.text_edit_updated});
      break;
    case MARU_EVENT_TEXT_EDIT_COMMITTED:
      if constexpr (std::invocable<F_raw, TextEditCommittedEvent>)
        f(TextEditCommittedEvent{window, evt.text_edit_committed});
      break;
    case MARU_EVENT_TEXT_EDIT_ENDED:
      if constexpr (std::invocable<F_raw, TextEditEndedEvent>)
        f(TextEditEndedEvent{window, evt.text_edit_ended});
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
