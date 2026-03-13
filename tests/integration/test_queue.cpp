#include "doctest/doctest.h"
#include "maru/maru.hpp"

TEST_CASE("Queue C++ API - Basic") {
    auto queue_res = maru::Queue::create(16);
    REQUIRE(queue_res.has_value());
    maru::Queue& queue = *queue_res;

    MARU_Event queued_evt = {};
    queue.push(MARU_EVENT_USER_0, 0, queued_evt);
    queue.commit();

    int count = 0;
    queue.scan(MARU_ALL_EVENTS, [](MARU_EventId type, MARU_WindowId window_id, const MARU_Event* evt, void* userdata) {
        (void)type; (void)window_id; (void)evt;
        (*(int*)userdata)++;
    }, &count);

    CHECK(count == 1);
}

#if __cplusplus >= 202002L
TEST_CASE("Queue C++ API - C++20 Visitor Scan") {
    auto queue_res = maru::Queue::create(16);
    REQUIRE(queue_res.has_value());
    maru::Queue& queue = *queue_res;

    MARU_Event frame_evt = {};
    frame_evt.window_frame.timestamp_ms = 123;
    MARU_Event close_evt = {};
    queue.push(MARU_EVENT_WINDOW_FRAME, MARU_WINDOW_ID_NONE, frame_evt);
    queue.push(MARU_EVENT_CLOSE_REQUESTED, MARU_WINDOW_ID_NONE, close_evt);
    queue.commit();

    // Scan with visitor, implicit mask should be generated
    int frame_count = 0;
    int close_count = 0;

    queue.scan(maru::overloads{
        [&](maru::QueuedWindowFrameEvent evt) {
            CHECK(evt->timestamp_ms == 123);
            frame_count++;
        },
        [&](maru::QueuedCloseRequestedEvent) { close_count++; }
    });

    CHECK(frame_count == 1);
    CHECK(close_count == 1);
}
#endif
