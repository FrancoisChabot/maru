// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_QUEUE_HPP_INCLUDED
#define MARU_CPP_QUEUE_HPP_INCLUDED

#include "maru/queue.h"
#include "maru/cpp/fwd.hpp"
#include "maru/cpp/expected.hpp"

namespace maru {

/**
 * @brief RAII wrapper for MARU_Queue.
 */
class Queue {
public:
    [[nodiscard]] static expected<Queue, bool> create(const MARU_QueueCreateInfo& create_info = MARU_QUEUE_CREATE_INFO_DEFAULT);
    [[nodiscard]] static expected<Queue, bool> create(uint32_t capacity);

    ~Queue();

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    Queue(Queue&& other) noexcept;
    Queue& operator=(Queue&& other) noexcept;

    MARU_Queue* get() const { return m_handle; }
    operator MARU_Queue*() const { return m_handle; }

    bool push(MARU_EventId type, MARU_WindowId window_id,
              const MARU_Event& event);

    bool commit();

    void scan(MARU_EventMask mask, MARU_QueueEventCallback callback,
              void* userdata = nullptr);

    void setCoalesceMask(MARU_EventMask mask) {
        maru_setQueueCoalesceMask(m_handle, mask);
    }

#if __cplusplus >= 202002L
    template <typename Visitor>
    void scan(EventDispatcher<Visitor>& dispatcher, MARU_EventMask mask = MARU_ALL_EVENTS);

    template <typename Visitor>
    void scan(Visitor&& visitor);
#endif

private:
    explicit Queue(MARU_Queue* handle) : m_handle(handle) {}
    MARU_Queue* m_handle = nullptr;
};

} // namespace maru

#endif // MARU_CPP_QUEUE_HPP_INCLUDED
