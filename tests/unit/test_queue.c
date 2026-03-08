#include "utest.h"
#include "maru/maru.h"
#include "maru/maru_queue.h"
#include "maru_test_utils.h"

struct QueueTestState {
    int event_count;
    MARU_EventId last_type;
};

static void on_queue_event(MARU_EventId type, MARU_Window *window, const MARU_Event *evt, void *userdata) {
    struct QueueTestState *state = (struct QueueTestState *)userdata;
    state->event_count++;
    state->last_type = type;
}

UTEST(QueueTest, CreateDestroy) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context *context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);

    MARU_Queue *queue = NULL;
    EXPECT_EQ(maru_queue_create(context, 16, &queue), MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);

    maru_queue_destroy(queue);
    maru_destroyContext(context);
}

UTEST(QueueTest, PumpCommitScan) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context *context = maru_test_createContext(&create_info);
    
    MARU_Queue *queue = NULL;
    maru_queue_create(context, 16, &queue);

    // 1. Post an event
    MARU_UserDefinedEvent user_evt = { .userdata = (void*)0x123 };
    maru_postEvent(context, MARU_EVENT_USER_0, NULL, user_evt);

    struct QueueTestState state = {0};

    // 2. Scan before pump/commit should find nothing
    maru_queue_scan(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 0);

    // 3. Pump events (this pulls from context into queue's active buffer)
    maru_queue_pump(queue, 0);
    
    // Scan still should find nothing because not committed
    maru_queue_scan(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 0);

    // 4. Commit
    EXPECT_EQ(maru_queue_commit(queue), MARU_SUCCESS);

    // 5. Scan should now find the event
    maru_queue_scan(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);
    EXPECT_EQ(state.last_type, MARU_EVENT_USER_0);

    maru_queue_destroy(queue);
    maru_destroyContext(context);
}

UTEST(QueueTest, MultipleCommits) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context *context = maru_test_createContext(&create_info);
    
    MARU_Queue *queue = NULL;
    maru_queue_create(context, 16, &queue);

    // Post Event A
    maru_postEvent(context, MARU_EVENT_USER_0, NULL, (MARU_UserDefinedEvent){0});
    maru_queue_pump(queue, 0);
    maru_queue_commit(queue);

    struct QueueTestState state = {0};
    maru_queue_scan(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);

    // Post Event B
    maru_postEvent(context, MARU_EVENT_USER_1, NULL, (MARU_UserDefinedEvent){0});
    maru_queue_pump(queue, 0);
    
    // Before second commit, scan still shows only A
    state.event_count = 0;
    maru_queue_scan(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);
    EXPECT_EQ(state.last_type, MARU_EVENT_USER_0);

    // Second commit: A is discarded, B becomes stable
    maru_queue_commit(queue);
    state.event_count = 0;
    maru_queue_scan(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);
    EXPECT_EQ(state.last_type, MARU_EVENT_USER_1);

    maru_queue_destroy(queue);
    maru_destroyContext(context);
}

UTEST(QueueTest, Masking) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context *context = maru_test_createContext(&create_info);
    
    MARU_Queue *queue = NULL;
    maru_queue_create(context, 16, &queue);

    maru_postEvent(context, MARU_EVENT_USER_0, NULL, (MARU_UserDefinedEvent){0});
    maru_postEvent(context, MARU_EVENT_USER_1, NULL, (MARU_UserDefinedEvent){0});
    maru_queue_pump(queue, 0);
    maru_queue_commit(queue);

    struct QueueTestState state = {0};
    // Scan only for USER_1
    maru_queue_scan(queue, MARU_MASK_USER_1, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);
    EXPECT_EQ(state.last_type, MARU_EVENT_USER_1);

    maru_queue_destroy(queue);
    maru_destroyContext(context);
}

struct CoalesceResult {
    int count;
    MARU_Vec2Dip pos;
    MARU_Vec2Dip delta;
};

static void on_coalesce_event(MARU_EventId type, MARU_Window *window, const MARU_Event *evt, void *userdata) {
    struct CoalesceResult *r = (struct CoalesceResult *)userdata;
    r->count++;
    if (type == MARU_EVENT_MOUSE_MOVED) {
        r->pos = evt->mouse_motion.position;
        r->delta = evt->mouse_motion.delta;
    }
}

UTEST(QueueTest, Coalescence) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context *context = maru_test_createContext(&create_info);
    MARU_Context_Base *ctx_base = (MARU_Context_Base *)context;
    
    MARU_Queue *queue = NULL;
    maru_queue_create(context, 16, &queue);

    maru_queue_set_coalesce_mask(queue, MARU_MASK_MOUSE_MOVED);

    // ev1
    MARU_Event ev1 = {0};
    ev1.mouse_motion.position.x = 10.0f;
    ev1.mouse_motion.position.y = 10.0f;
    ev1.mouse_motion.delta.x = 5.0f;
    ev1.mouse_motion.delta.y = 5.0f;
    _maru_post_event_internal(ctx_base, MARU_EVENT_MOUSE_MOVED, NULL, &ev1);

    // ev2
    MARU_Event ev2 = {0};
    ev2.mouse_motion.position.x = 20.0f;
    ev2.mouse_motion.position.y = 20.0f;
    ev2.mouse_motion.delta.x = 10.0f;
    ev2.mouse_motion.delta.y = 10.0f;
    _maru_post_event_internal(ctx_base, MARU_EVENT_MOUSE_MOVED, NULL, &ev2);

    maru_queue_pump(queue, 0);
    maru_queue_commit(queue);

    struct CoalesceResult result = {0};
    maru_queue_scan(queue, MARU_ALL_EVENTS, on_coalesce_event, &result);

    EXPECT_EQ(result.count, 1);
    EXPECT_EQ(result.pos.x, 20.0f);
    EXPECT_EQ(result.pos.y, 20.0f);
    EXPECT_EQ(result.delta.x, 15.0f);
    EXPECT_EQ(result.delta.y, 15.0f);

    maru_queue_destroy(queue);
    maru_destroyContext(context);
}
