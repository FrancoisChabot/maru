// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_CXX20_HPP_INCLUDED
#define MARU_CPP_CXX20_HPP_INCLUDED

#include "maru/maru.h"
#include "maru/cpp/events.hpp"

#include <concepts>
#include <type_traits>

namespace maru {

/** Utility for creating a visitor from multiple lambdas. */
template <class... Ts>
struct overloads : Ts... {
  using Ts::operator()...;
};

template <class... Ts>
overloads(Ts...) -> overloads<Ts...>;

template <typename T>
concept PartialEventVisitor =
  std::invocable<std::remove_cvref_t<T>, WindowReadyEvent> ||
  std::invocable<std::remove_cvref_t<T>, CloseRequestedEvent> ||
  std::invocable<std::remove_cvref_t<T>, WindowResizedEvent> ||
  std::invocable<std::remove_cvref_t<T>, WindowStateChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, KeyChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, MouseMovedEvent> ||
  std::invocable<std::remove_cvref_t<T>, MouseButtonChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, MouseScrolledEvent> ||
  std::invocable<std::remove_cvref_t<T>, IdleEvent> ||
  std::invocable<std::remove_cvref_t<T>, MonitorChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, MonitorModeEvent> ||
  std::invocable<std::remove_cvref_t<T>, DropEnteredEvent> ||
  std::invocable<std::remove_cvref_t<T>, DropHoveredEvent> ||
  std::invocable<std::remove_cvref_t<T>, DropExitedEvent> ||
  std::invocable<std::remove_cvref_t<T>, DropDroppedEvent> ||
  std::invocable<std::remove_cvref_t<T>, DataReceivedEvent> ||
  std::invocable<std::remove_cvref_t<T>, DataRequestEvent> ||
  std::invocable<std::remove_cvref_t<T>, DataConsumedEvent> ||
  std::invocable<std::remove_cvref_t<T>, DragFinishedEvent> ||
  std::invocable<std::remove_cvref_t<T>, ControllerChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, ControllerButtonChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, WindowFrameEvent> ||
  std::invocable<std::remove_cvref_t<T>, TextEditStartedEvent> ||
  std::invocable<std::remove_cvref_t<T>, TextEditUpdatedEvent> ||
  std::invocable<std::remove_cvref_t<T>, TextEditCommittedEvent> ||
  std::invocable<std::remove_cvref_t<T>, TextEditEndedEvent>;

template <typename T>
concept PartialQueueEventVisitor =
  std::invocable<std::remove_cvref_t<T>, QueuedWindowReadyEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedCloseRequestedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedWindowResizedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedWindowStateChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedKeyChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedMouseMovedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedMouseButtonChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedMouseScrolledEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedIdleEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedMonitorChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedMonitorModeEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedDropEnteredEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedDropHoveredEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedDropExitedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedDropDroppedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedDataReceivedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedDataRequestEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedDataConsumedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedDragFinishedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedControllerChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedControllerButtonChangedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedWindowFrameEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedTextEditStartedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedTextEditUpdatedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedTextEditCommittedEvent> ||
  std::invocable<std::remove_cvref_t<T>, QueuedTextEditEndedEvent>;

/**
 * A helper class that can be used as event_userdata to dispatch events to visitors.
 */
template <typename Visitor>
class EventDispatcher {
 public:
  explicit EventDispatcher(Visitor &&visitor) : m_visitor(std::forward<Visitor>(visitor)) {}

  static void callback(MARU_EventId type, MARU_Window *window, const MARU_Event *event,
                       void *userdata);
  static void queueCallback(MARU_EventId type, MARU_WindowId window_id,
                            const MARU_Event *event, void *userdata);

 private:
  Visitor m_visitor;
};

/**
 * Creates an EventDispatcher from one or more handlers.
 */
template <typename... Fs>
auto makeDispatcher(Fs &&...handlers) {
    return EventDispatcher<overloads<std::remove_cvref_t<Fs>...>>(
        overloads{std::forward<Fs>(handlers)...});
}

}  // namespace maru

#include "maru/details/maru_cxx20_impl.hpp"

namespace maru {

template <typename Visitor>
inline MARU_Status Context::pumpEvents(EventDispatcher<Visitor>& dispatcher,
                                       uint32_t timeout_ms,
                                       MARU_EventMask mask) {
  return pumpEvents(timeout_ms, mask, dispatcher.callback, &dispatcher);
}

template <typename Visitor>
inline void Queue::scan(EventDispatcher<Visitor>& dispatcher, MARU_EventMask mask) {
  scan(mask, dispatcher.queueCallback, &dispatcher);
}

template <typename Visitor>
inline void Queue::scan(Visitor&& visitor) {
  EventDispatcher<std::remove_cvref_t<Visitor>> dispatcher(std::forward<Visitor>(visitor));
  scan(dispatcher, details::generateMask<std::remove_cvref_t<Visitor>>());
}

}  // namespace maru

#endif  // MARU_CPP_CXX20_HPP_INCLUDED
