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
  if constexpr (std::invocable<F, WindowReadyEvent> || std::invocable<F, QueuedWindowReadyEvent>)
    mask |= MARU_MASK_WINDOW_READY;
  if constexpr (std::invocable<F, CloseRequestedEvent> || std::invocable<F, QueuedCloseRequestedEvent>)
    mask |= MARU_MASK_CLOSE_REQUESTED;
  if constexpr (std::invocable<F, WindowResizedEvent> || std::invocable<F, QueuedWindowResizedEvent>)
    mask |= MARU_MASK_WINDOW_RESIZED;
  if constexpr (std::invocable<F, WindowStateChangedEvent> ||
                std::invocable<F, QueuedWindowStateChangedEvent>)
    mask |= MARU_MASK_WINDOW_STATE_CHANGED;
  if constexpr (std::invocable<F, KeyChangedEvent> || std::invocable<F, QueuedKeyChangedEvent>)
    mask |= MARU_MASK_KEY_CHANGED;
  if constexpr (std::invocable<F, MouseMovedEvent> || std::invocable<F, QueuedMouseMovedEvent>)
    mask |= MARU_MASK_MOUSE_MOVED;
  if constexpr (std::invocable<F, MouseButtonChangedEvent> ||
                std::invocable<F, QueuedMouseButtonChangedEvent>)
    mask |= MARU_MASK_MOUSE_BUTTON_CHANGED;
  if constexpr (std::invocable<F, MouseScrolledEvent> || std::invocable<F, QueuedMouseScrolledEvent>)
    mask |= MARU_MASK_MOUSE_SCROLLED;
  if constexpr (std::invocable<F, IdleEvent> || std::invocable<F, QueuedIdleEvent>)
    mask |= MARU_MASK_IDLE_CHANGED;
  if constexpr (std::invocable<F, MonitorChangedEvent> || std::invocable<F, QueuedMonitorChangedEvent>)
    mask |= MARU_MASK_MONITOR_CHANGED;
  if constexpr (std::invocable<F, MonitorModeEvent> || std::invocable<F, QueuedMonitorModeEvent>)
    mask |= MARU_MASK_MONITOR_MODE_CHANGED;
  if constexpr (std::invocable<F, DropEnteredEvent> || std::invocable<F, QueuedDropEnteredEvent>)
    mask |= MARU_MASK_DROP_ENTERED;
  if constexpr (std::invocable<F, DropHoveredEvent> || std::invocable<F, QueuedDropHoveredEvent>)
    mask |= MARU_MASK_DROP_HOVERED;
  if constexpr (std::invocable<F, DropExitedEvent> || std::invocable<F, QueuedDropExitedEvent>)
    mask |= MARU_MASK_DROP_EXITED;
  if constexpr (std::invocable<F, DropDroppedEvent> || std::invocable<F, QueuedDropDroppedEvent>)
    mask |= MARU_MASK_DROP_DROPPED;
  if constexpr (std::invocable<F, DataReceivedEvent> || std::invocable<F, QueuedDataReceivedEvent>)
    mask |= MARU_EVENT_MASK(MARU_EVENT_DATA_RECEIVED);
  if constexpr (std::invocable<F, DataRequestEvent> || std::invocable<F, QueuedDataRequestEvent>)
    mask |= MARU_EVENT_MASK(MARU_EVENT_DATA_REQUESTED);
  if constexpr (std::invocable<F, DataConsumedEvent> || std::invocable<F, QueuedDataConsumedEvent>)
    mask |= MARU_EVENT_MASK(MARU_EVENT_DATA_CONSUMED);
  if constexpr (std::invocable<F, DragFinishedEvent> || std::invocable<F, QueuedDragFinishedEvent>)
    mask |= MARU_EVENT_MASK(MARU_EVENT_DRAG_FINISHED);
  if constexpr (std::invocable<F, ControllerChangedEvent> ||
                std::invocable<F, QueuedControllerChangedEvent>)
    mask |= MARU_MASK_CONTROLLER_CHANGED;
  if constexpr (std::invocable<F, ControllerButtonChangedEvent> ||
                std::invocable<F, QueuedControllerButtonChangedEvent>)
    mask |= MARU_EVENT_MASK(MARU_EVENT_CONTROLLER_BUTTON_CHANGED);
  if constexpr (std::invocable<F, WindowFrameEvent> || std::invocable<F, QueuedWindowFrameEvent>)
    mask |= MARU_MASK_WINDOW_FRAME;
  if constexpr (std::invocable<F, TextEditStartedEvent> ||
                std::invocable<F, QueuedTextEditStartedEvent>)
    mask |= MARU_MASK_TEXT_EDIT_STARTED;
  if constexpr (std::invocable<F, TextEditUpdatedEvent> ||
                std::invocable<F, QueuedTextEditUpdatedEvent>)
    mask |= MARU_MASK_TEXT_EDIT_UPDATED;
  if constexpr (std::invocable<F, TextEditCommittedEvent> ||
                std::invocable<F, QueuedTextEditCommittedEvent>)
    mask |= MARU_MASK_TEXT_EDIT_COMMITTED;
  if constexpr (std::invocable<F, TextEditEndedEvent> || std::invocable<F, QueuedTextEditEndedEvent>)
    mask |= MARU_MASK_TEXT_EDIT_ENDED;
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
        f(WindowResizedEvent{window, evt.window_resized});
      break;
    case MARU_EVENT_WINDOW_STATE_CHANGED:
      if constexpr (std::invocable<F_raw, WindowStateChangedEvent>)
        f(WindowStateChangedEvent{window, evt.window_state_changed});
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
        f(WindowFrameEvent{window, evt.window_frame});
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

template <typename Visitor>
void dispatchQueuedVisitor(MARU_EventId type, MARU_WindowId window_id,
                           const MARU_Event &evt, Visitor &&f) {
  using F_raw = std::remove_cvref_t<Visitor>;
  switch (type) {
    case MARU_EVENT_WINDOW_READY:
      if constexpr (std::invocable<F_raw, QueuedWindowReadyEvent>)
        f(QueuedWindowReadyEvent{window_id, evt.window_ready});
      break;
    case MARU_EVENT_CLOSE_REQUESTED:
      if constexpr (std::invocable<F_raw, QueuedCloseRequestedEvent>)
        f(QueuedCloseRequestedEvent{window_id, evt.close_requested});
      break;
    case MARU_EVENT_WINDOW_RESIZED:
      if constexpr (std::invocable<F_raw, QueuedWindowResizedEvent>)
        f(QueuedWindowResizedEvent{window_id, evt.window_resized});
      break;
    case MARU_EVENT_WINDOW_STATE_CHANGED:
      if constexpr (std::invocable<F_raw, QueuedWindowStateChangedEvent>)
        f(QueuedWindowStateChangedEvent{window_id, evt.window_state_changed});
      break;
    case MARU_EVENT_KEY_CHANGED:
      if constexpr (std::invocable<F_raw, QueuedKeyChangedEvent>)
        f(QueuedKeyChangedEvent{window_id, evt.key_changed});
      break;
    case MARU_EVENT_MOUSE_MOVED:
      if constexpr (std::invocable<F_raw, QueuedMouseMovedEvent>)
        f(QueuedMouseMovedEvent{window_id, evt.mouse_moved});
      break;
    case MARU_EVENT_MOUSE_BUTTON_CHANGED:
      if constexpr (std::invocable<F_raw, QueuedMouseButtonChangedEvent>)
        f(QueuedMouseButtonChangedEvent{window_id, evt.mouse_button_changed});
      break;
    case MARU_EVENT_MOUSE_SCROLLED:
      if constexpr (std::invocable<F_raw, QueuedMouseScrolledEvent>)
        f(QueuedMouseScrolledEvent{window_id, evt.mouse_scrolled});
      break;
    case MARU_EVENT_IDLE_CHANGED:
      if constexpr (std::invocable<F_raw, QueuedIdleEvent>)
        f(QueuedIdleEvent{window_id, evt.idle_changed});
      break;
    case MARU_EVENT_MONITOR_CHANGED:
      if constexpr (std::invocable<F_raw, QueuedMonitorChangedEvent>)
        f(QueuedMonitorChangedEvent{window_id, evt.monitor_changed});
      break;
    case MARU_EVENT_MONITOR_MODE_CHANGED:
      if constexpr (std::invocable<F_raw, QueuedMonitorModeEvent>)
        f(QueuedMonitorModeEvent{window_id, evt.monitor_mode_changed});
      break;
    case MARU_EVENT_DROP_ENTERED:
      if constexpr (std::invocable<F_raw, QueuedDropEnteredEvent>)
        f(QueuedDropEnteredEvent{window_id, evt.drop_entered});
      break;
    case MARU_EVENT_DROP_HOVERED:
      if constexpr (std::invocable<F_raw, QueuedDropHoveredEvent>)
        f(QueuedDropHoveredEvent{window_id, evt.drop_hovered});
      break;
    case MARU_EVENT_DROP_EXITED:
      if constexpr (std::invocable<F_raw, QueuedDropExitedEvent>)
        f(QueuedDropExitedEvent{window_id, evt.drop_exited});
      break;
    case MARU_EVENT_DROP_DROPPED:
      if constexpr (std::invocable<F_raw, QueuedDropDroppedEvent>)
        f(QueuedDropDroppedEvent{window_id, evt.drop_dropped});
      break;
    case MARU_EVENT_DATA_RECEIVED:
      if constexpr (std::invocable<F_raw, QueuedDataReceivedEvent>)
        f(QueuedDataReceivedEvent{window_id, evt.data_received});
      break;
    case MARU_EVENT_DATA_REQUESTED:
      if constexpr (std::invocable<F_raw, QueuedDataRequestEvent>)
        f(QueuedDataRequestEvent{window_id, evt.data_requested});
      break;
    case MARU_EVENT_DATA_CONSUMED:
      if constexpr (std::invocable<F_raw, QueuedDataConsumedEvent>)
        f(QueuedDataConsumedEvent{window_id, evt.data_consumed});
      break;
    case MARU_EVENT_DRAG_FINISHED:
      if constexpr (std::invocable<F_raw, QueuedDragFinishedEvent>)
        f(QueuedDragFinishedEvent{window_id, evt.drag_finished});
      break;
    case MARU_EVENT_CONTROLLER_CHANGED:
      if constexpr (std::invocable<F_raw, QueuedControllerChangedEvent>)
        f(QueuedControllerChangedEvent{window_id, evt.controller_changed});
      break;
    case MARU_EVENT_CONTROLLER_BUTTON_CHANGED:
      if constexpr (std::invocable<F_raw, QueuedControllerButtonChangedEvent>)
        f(QueuedControllerButtonChangedEvent{window_id, evt.controller_button_changed});
      break;
    case MARU_EVENT_WINDOW_FRAME:
      if constexpr (std::invocable<F_raw, QueuedWindowFrameEvent>)
        f(QueuedWindowFrameEvent{window_id, evt.window_frame});
      break;
    case MARU_EVENT_TEXT_EDIT_STARTED:
      if constexpr (std::invocable<F_raw, QueuedTextEditStartedEvent>)
        f(QueuedTextEditStartedEvent{window_id, evt.text_edit_started});
      break;
    case MARU_EVENT_TEXT_EDIT_UPDATED:
      if constexpr (std::invocable<F_raw, QueuedTextEditUpdatedEvent>)
        f(QueuedTextEditUpdatedEvent{window_id, evt.text_edit_updated});
      break;
    case MARU_EVENT_TEXT_EDIT_COMMITTED:
      if constexpr (std::invocable<F_raw, QueuedTextEditCommittedEvent>)
        f(QueuedTextEditCommittedEvent{window_id, evt.text_edit_committed});
      break;
    case MARU_EVENT_TEXT_EDIT_ENDED:
      if constexpr (std::invocable<F_raw, QueuedTextEditEndedEvent>)
        f(QueuedTextEditEndedEvent{window_id, evt.text_edit_ended});
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

template <typename Visitor>
void EventDispatcher<Visitor>::queueCallback(MARU_EventId type,
                                             MARU_WindowId window_id,
                                             const MARU_Event *event,
                                             void *userdata) {
  auto *self = static_cast<EventDispatcher<Visitor> *>(userdata);
  if constexpr (PartialQueueEventVisitor<Visitor>) {
    details::dispatchQueuedVisitor(type, window_id, *event, self->m_visitor);
  } else if constexpr (std::invocable<Visitor, MARU_EventId, MARU_WindowId,
                                      const MARU_Event &>) {
    self->m_visitor(type, window_id, *event);
  }
}

}  // namespace maru

#endif  // MARU_CXX20_IMPL_HPP_INCLUDED
