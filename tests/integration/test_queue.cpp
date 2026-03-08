#include "doctest/doctest.h"
#include "maru/maru.hpp"
#include <vector>

TEST_CASE("Queue C++ API - Basic") {
    maru::Context ctx;
    maru::Queue queue(ctx, 16);

    maru_postEvent(ctx.get(), MARU_EVENT_USER_0, nullptr, (MARU_UserDefinedEvent){0});
    queue.pump();
    queue.commit();

    int count = 0;
    queue.scan(MARU_ALL_EVENTS, [](MARU_EventId type, MARU_Window* window, const MARU_Event* evt, void* userdata) {
        (*(int*)userdata)++;
    }, &count);

    CHECK(count == 1);
}

#if __cplusplus >= 202002L
TEST_CASE("Queue C++ API - C++20 Visitor Scan") {
    maru::Context ctx;
    maru::Queue queue(ctx, 16);

    // Post some events
    maru_postEvent(ctx.get(), MARU_EVENT_WINDOW_READY, nullptr, (MARU_Event){.window_ready = {0}});
    maru_postEvent(ctx.get(), MARU_EVENT_CLOSE_REQUESTED, nullptr, (MARU_Event){.close_requested = {0}});
    
    queue.pump();
    queue.commit();

    int ready_count = 0;
    int close_count = 0;

    // Scan with visitor, implicit mask should be generated
    queue.scan(maru::overloads{
        [&](maru::WindowReadyEvent) { ready_count++; },
        [&](maru::WindowCloseEvent) { close_count++; }
    });

    CHECK(ready_count == 1);
    CHECK(close_count == 1);
}
#endif
