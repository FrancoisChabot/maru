#include "doctest/doctest.h"
#include "maru/maru.hpp"

TEST_CASE("Queue C++ API - Basic") {
    auto ctx_res = maru::Context::create();
    if (!ctx_res.has_value()) {
        MESSAGE("Context creation unavailable; skipping queue test.");
        return;
    }
    maru::Context& ctx = *ctx_res;

    auto queue_res = maru::Queue::create(ctx, 16);
    REQUIRE(queue_res.has_value());
    maru::Queue& queue = *queue_res;

    MARU_Event queued_evt = {};
    queue.push(MARU_EVENT_USER_0, nullptr, queued_evt);
    queue.commit();

    int count = 0;
    queue.scan(MARU_ALL_EVENTS, [](MARU_EventId type, MARU_Window* window, const MARU_Event* evt, void* userdata) {
        (void)type; (void)window; (void)evt;
        (*(int*)userdata)++;
    }, &count);

    CHECK(count == 1);
}

TEST_CASE("Queue C++ API - Overflow Metrics") {
    auto ctx_res = maru::Context::create();
    if (!ctx_res.has_value()) {
        MESSAGE("Context creation unavailable; skipping queue test.");
        return;
    }
    maru::Context& ctx = *ctx_res;

    auto queue_res = maru::Queue::create(ctx, 2);
    REQUIRE(queue_res.has_value());
    maru::Queue& queue = *queue_res;

    MARU_Event queued_evt = {};
    queue.push(MARU_EVENT_USER_0, nullptr, queued_evt);
    queue.push(MARU_EVENT_USER_1, nullptr, queued_evt);
    queue.push(MARU_EVENT_USER_2, nullptr, queued_evt);
    queue.commit();

    MARU_QueueMetrics metrics = queue.metrics();
    CHECK(metrics.peak_active_count == 2u);
    CHECK(metrics.overflow_drop_count == 1u);
    CHECK(metrics.overflow_compact_count == 1u);
    CHECK(metrics.overflow_events_compacted == 0u);
    CHECK(metrics.overflow_drop_count_by_event[MARU_EVENT_USER_2] == 1u);

    queue.resetMetrics();
    metrics = queue.metrics();
    CHECK(metrics.peak_active_count == 0u);
    CHECK(metrics.overflow_drop_count == 0u);
    CHECK(metrics.overflow_drop_count_by_event[MARU_EVENT_USER_2] == 0u);
}

#if __cplusplus >= 202002L
TEST_CASE("Queue C++ API - C++20 Visitor Scan") {
    auto ctx_res = maru::Context::create();
    if (!ctx_res.has_value()) {
        MESSAGE("Context creation unavailable; skipping queue test.");
        return;
    }
    maru::Context& ctx = *ctx_res;

    auto queue_res = maru::Queue::create(ctx, 16);
    REQUIRE(queue_res.has_value());
    maru::Queue& queue = *queue_res;

    MARU_Event ready_evt = {};
    MARU_Event close_evt = {};
    queue.push(MARU_EVENT_WINDOW_READY, nullptr, ready_evt);
    queue.push(MARU_EVENT_CLOSE_REQUESTED, nullptr, close_evt);
    queue.commit();

    // Scan with visitor, implicit mask should be generated
    int ready_count = 0;
    int close_count = 0;

    queue.scan(maru::overloads{
        [&](maru::WindowReadyEvent) { ready_count++; },
        [&](maru::CloseRequestedEvent) { close_count++; }
    });

    CHECK(ready_count == 1);
    CHECK(close_count == 1);
}
#endif
