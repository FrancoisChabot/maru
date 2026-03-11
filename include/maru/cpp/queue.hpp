// SPDX-License-Identifier: Zlib
// Copyright (c) 2026 François Chabot

#ifndef MARU_CPP_QUEUE_HPP_INCLUDED
#define MARU_CPP_QUEUE_HPP_INCLUDED

#include "maru/maru.h"
#include "maru/cpp/fwd.hpp"
#include "maru/cpp/expected.hpp"
#include "maru/cpp/context.hpp"

namespace maru {

/**
 * @brief RAII wrapper for MARU_Queue.
 */
class Queue {
public:
    [[nodiscard]] static expected<Queue> create(Context& ctx, uint32_t capacity_power_of_2);

    ~Queue();

    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    Queue(Queue&& other) noexcept;
    Queue& operator=(Queue&& other) noexcept;

    MARU_Queue* get() const { return m_handle; }
    operator MARU_Queue*() const { return m_handle; }

    MARU_Status push(MARU_EventId type, MARU_Window* window,
                     const MARU_Event& event);

    MARU_Status commit();

    void scan(MARU_EventMask mask, MARU_EventCallback callback, void* userdata = nullptr);

    void setCoalesceMask(MARU_EventMask mask) {
        maru_queue_set_coalesce_mask(m_handle, mask);
    }

    MARU_QueueMetrics metrics() const;
    void resetMetrics();

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
