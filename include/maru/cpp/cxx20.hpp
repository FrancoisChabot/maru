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
concept PartialEventVisitor = std::invocable<std::remove_cvref_t<T>, WindowReadyEvent> ||
                              std::invocable<std::remove_cvref_t<T>, WindowCloseEvent> ||
                              std::invocable<std::remove_cvref_t<T>, WindowResizedEvent> ||
                              std::invocable<std::remove_cvref_t<T>, WindowPresentationStateEvent> ||
                              std::invocable<std::remove_cvref_t<T>, KeyboardEvent> ||
                              std::invocable<std::remove_cvref_t<T>, MouseMotionEvent> ||
                              std::invocable<std::remove_cvref_t<T>, MouseButtonEvent> ||
                              std::invocable<std::remove_cvref_t<T>, MouseScrollEvent> ||
                              std::invocable<std::remove_cvref_t<T>, IdleEvent> ||
                              std::invocable<std::remove_cvref_t<T>, MonitorConnectionEvent> ||
                              std::invocable<std::remove_cvref_t<T>, MonitorModeEvent> ||
                              std::invocable<std::remove_cvref_t<T>, DropEnterEvent> ||
                              std::invocable<std::remove_cvref_t<T>, DropHoverEvent> ||
                              std::invocable<std::remove_cvref_t<T>, DropLeaveEvent> ||
                              std::invocable<std::remove_cvref_t<T>, DropEvent> ||
                              std::invocable<std::remove_cvref_t<T>, DataReceivedEvent> ||
                              std::invocable<std::remove_cvref_t<T>, DataRequestEvent> ||
                              std::invocable<std::remove_cvref_t<T>, DataConsumedEvent> ||
                              std::invocable<std::remove_cvref_t<T>, DragFinishedEvent> ||
                              std::invocable<std::remove_cvref_t<T>, ControllerConnectionEvent> ||
                              std::invocable<std::remove_cvref_t<T>, ControllerButtonStateChangedEvent> ||
                              std::invocable<std::remove_cvref_t<T>, WindowFrameEvent> ||
                              std::invocable<std::remove_cvref_t<T>, TextEditStartEvent> ||
                              std::invocable<std::remove_cvref_t<T>, TextEditUpdateEvent> ||
                              std::invocable<std::remove_cvref_t<T>, TextEditCommitEvent> ||
                              std::invocable<std::remove_cvref_t<T>, TextEditEndEvent>;

/**
 * A helper class that can be used as event_userdata to dispatch events to visitors.
 */
template <typename Visitor>
class EventDispatcher {
 public:
  explicit EventDispatcher(Visitor &&visitor) : m_visitor(std::forward<Visitor>(visitor)) {}

  static void callback(MARU_EventId type, MARU_Window *window, const MARU_Event *event,
                       void *userdata);

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

template <typename Visitor>
inline void Context::pumpEvents(EventDispatcher<Visitor>& dispatcher, uint32_t timeout_ms) {
    pumpEvents(timeout_ms, dispatcher.callback, &dispatcher);
}

}  // namespace maru

#include "maru/details/maru_cxx20_impl.hpp"

#endif  // MARU_CPP_CXX20_HPP_INCLUDED
