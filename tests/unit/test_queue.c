#include "utest.h"
#include "maru/maru.h"
#include "maru_test_utils.h"
#include <stdlib.h>

struct QueueTestState {
    int event_count;
    MARU_EventId last_type;
    MARU_WindowId last_window_id;
};

static void on_queue_event(MARU_EventId type,
                           MARU_WindowId window_id,
                           const MARU_Event *evt,
                           void *userdata) {
    struct QueueTestState *state = (struct QueueTestState *)userdata;
    (void)evt;
    state->event_count++;
    state->last_type = type;
    state->last_window_id = window_id;
}

static MARU_Status push_user_event(MARU_Queue *queue, MARU_EventId type) {
    MARU_Event evt = {0};
    evt.user = (MARU_UserDefinedEvent){0};
    return maru_pushQueue(queue, type, MARU_WINDOW_ID_NONE, &evt);
}

static MARU_Status create_queue(uint32_t capacity, MARU_Queue **out_queue) {
    MARU_QueueCreateInfo create_info = MARU_QUEUE_CREATE_INFO_DEFAULT;
    create_info.capacity = capacity;
    return maru_createQueue(&create_info, out_queue);
}

UTEST(QueueTest, CreateDestroy) {
    MARU_Queue *queue = NULL;
    EXPECT_EQ(create_queue(16, &queue), (MARU_Status)MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);

    maru_destroyQueue(queue);
}

UTEST(QueueTest, PushCommitScan) {
    MARU_Queue *queue = NULL;
    EXPECT_EQ(create_queue(256u, &queue), (MARU_Status)MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);

    // 1. Push an event
    MARU_Event evt = {0};
    evt.user.userdata = (void *)0x123;
    maru_pushQueue(queue, MARU_EVENT_USER_0, MARU_WINDOW_ID_NONE, &evt);

    struct QueueTestState state = {0};

    // 2. Scan before commit should find nothing
    maru_scanQueue(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 0);

    // 4. Commit
    EXPECT_EQ(maru_commitQueue(queue), (MARU_Status)MARU_SUCCESS);

    // 5. Scan should now find the event
    maru_scanQueue(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);
    EXPECT_EQ(state.last_type, (MARU_EventId)MARU_EVENT_USER_0);
    EXPECT_EQ(state.last_window_id, (MARU_WindowId)MARU_WINDOW_ID_NONE);

    maru_destroyQueue(queue);
}

UTEST(QueueTest, MultipleCommits) {
    MARU_Queue *queue = NULL;
    EXPECT_EQ(create_queue(16, &queue), (MARU_Status)MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);

    // Push Event A
    EXPECT_EQ(push_user_event(queue, MARU_EVENT_USER_0),
              (MARU_Status)MARU_SUCCESS);
    maru_commitQueue(queue);

    struct QueueTestState state = {0};
    maru_scanQueue(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);

    // Push Event B
    EXPECT_EQ(push_user_event(queue, MARU_EVENT_USER_1),
              (MARU_Status)MARU_SUCCESS);
    
    // Before second commit, scan still shows only A
    state.event_count = 0;
    maru_scanQueue(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);
    EXPECT_EQ(state.last_type, (MARU_EventId)MARU_EVENT_USER_0);

    // Second commit: A is discarded, B becomes stable
    maru_commitQueue(queue);
    state.event_count = 0;
    maru_scanQueue(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);
    EXPECT_EQ(state.last_type, (MARU_EventId)MARU_EVENT_USER_1);

    maru_destroyQueue(queue);
}

UTEST(QueueTest, Masking) {
    MARU_Queue *queue = NULL;
    EXPECT_EQ(create_queue(16, &queue), (MARU_Status)MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);

    EXPECT_EQ(push_user_event(queue, MARU_EVENT_USER_0),
              (MARU_Status)MARU_SUCCESS);
    EXPECT_EQ(push_user_event(queue, MARU_EVENT_USER_1),
              (MARU_Status)MARU_SUCCESS);
    maru_commitQueue(queue);

    struct QueueTestState state = {0};
    // Scan only for USER_1
    maru_scanQueue(queue, MARU_MASK_USER_1, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);
    EXPECT_EQ(state.last_type, (MARU_EventId)MARU_EVENT_USER_1);

    maru_destroyQueue(queue);
}

UTEST(QueueTest, QueueSafeMaskHelper) {
    EXPECT_TRUE(maru_isQueueSafeEventId(MARU_EVENT_USER_0));
    EXPECT_TRUE(maru_isQueueSafeEventId(MARU_EVENT_WINDOW_FRAME));
    EXPECT_TRUE(maru_isQueueSafeEventId(MARU_EVENT_DRAG_FINISHED));
    EXPECT_FALSE(maru_isQueueSafeEventId(MARU_EVENT_DATA_REQUESTED));
    EXPECT_FALSE(maru_isQueueSafeEventId(MARU_EVENT_MONITOR_CHANGED));
    EXPECT_FALSE(maru_isQueueSafeEventId(MARU_EVENT_CONTROLLER_CHANGED));
    EXPECT_FALSE(maru_isQueueSafeEventId(MARU_EVENT_WINDOW_STATE_CHANGED));
}

struct CoalesceResult {
    int count;
    MARU_Vec2Dip dip_pos;
    MARU_Vec2Dip dip_delta;
};

static void on_coalesce_event(MARU_EventId type,
                              MARU_WindowId window_id,
                              const MARU_Event *evt,
                              void *userdata) {
    struct CoalesceResult *r = (struct CoalesceResult *)userdata;
    (void)window_id;
    r->count++;
    if (type == MARU_EVENT_MOUSE_MOVED) {
        r->dip_pos = evt->mouse_moved.dip_position;
        r->dip_delta = evt->mouse_moved.dip_delta;
    }
}

UTEST(QueueTest, Coalescence) {
    MARU_Queue *queue = NULL;
    EXPECT_EQ(create_queue(16, &queue), (MARU_Status)MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);

    maru_setQueueCoalesceMask(queue, MARU_MASK_MOUSE_MOVED);

    // ev1
    MARU_Event ev1 = {0};
    ev1.mouse_moved.dip_position.x = (MARU_Scalar)10.0;
    ev1.mouse_moved.dip_position.y = (MARU_Scalar)10.0;
    ev1.mouse_moved.dip_delta.x = (MARU_Scalar)5.0;
    ev1.mouse_moved.dip_delta.y = (MARU_Scalar)5.0;
    maru_pushQueue(queue, MARU_EVENT_MOUSE_MOVED, MARU_WINDOW_ID_NONE, &ev1);

    // ev2
    MARU_Event ev2 = {0};
    ev2.mouse_moved.dip_position.x = (MARU_Scalar)20.0;
    ev2.mouse_moved.dip_position.y = (MARU_Scalar)20.0;
    ev2.mouse_moved.dip_delta.x = (MARU_Scalar)10.0;
    ev2.mouse_moved.dip_delta.y = (MARU_Scalar)10.0;
    maru_pushQueue(queue, MARU_EVENT_MOUSE_MOVED, MARU_WINDOW_ID_NONE, &ev2);

    maru_commitQueue(queue);

    struct CoalesceResult result = {0};
    maru_scanQueue(queue, MARU_ALL_EVENTS, on_coalesce_event, &result);

    EXPECT_EQ(result.count, 1);
    EXPECT_EQ(result.dip_pos.x, (MARU_Scalar)20.0);
    EXPECT_EQ(result.dip_pos.y, (MARU_Scalar)20.0);
    EXPECT_EQ(result.dip_delta.x, (MARU_Scalar)15.0);
    EXPECT_EQ(result.dip_delta.y, (MARU_Scalar)15.0);

    maru_destroyQueue(queue);
}

struct QueueTrace {
    uint32_t count;
    MARU_EventId types[16];
    MARU_Event events[16];
};

static void on_trace_event(MARU_EventId type,
                           MARU_WindowId window_id,
                           const MARU_Event *evt,
                           void *userdata) {
    (void)window_id;
    struct QueueTrace *trace = (struct QueueTrace *)userdata;
    if (trace->count >= 16u) return;
    uint32_t idx = trace->count++;
    trace->types[idx] = type;
    trace->events[idx] = *evt;
}

UTEST(QueueTest, OverflowCompactionAccumulatesAndPreservesOrder) {
    MARU_Queue *queue = NULL;
    EXPECT_EQ(create_queue(3, &queue), (MARU_Status)MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);
    maru_setQueueCoalesceMask(queue, MARU_MASK_MOUSE_MOVED);

    MARU_Event move_a = {0};
    move_a.mouse_moved.dip_position.x = (MARU_Scalar)10.0;
    move_a.mouse_moved.dip_position.y = (MARU_Scalar)10.0;
    move_a.mouse_moved.dip_delta.x = (MARU_Scalar)1.0;
    move_a.mouse_moved.dip_delta.y = (MARU_Scalar)2.0;
    move_a.mouse_moved.raw_dip_delta.x = (MARU_Scalar)3.0;
    move_a.mouse_moved.raw_dip_delta.y = (MARU_Scalar)4.0;
    maru_pushQueue(queue, MARU_EVENT_MOUSE_MOVED, MARU_WINDOW_ID_NONE, &move_a);

    push_user_event(queue, MARU_EVENT_USER_0);

    MARU_Event move_b = {0};
    move_b.mouse_moved.dip_position.x = (MARU_Scalar)20.0;
    move_b.mouse_moved.dip_position.y = (MARU_Scalar)30.0;
    move_b.mouse_moved.dip_delta.x = (MARU_Scalar)5.0;
    move_b.mouse_moved.dip_delta.y = (MARU_Scalar)7.0;
    move_b.mouse_moved.raw_dip_delta.x = (MARU_Scalar)11.0;
    move_b.mouse_moved.raw_dip_delta.y = (MARU_Scalar)13.0;
    maru_pushQueue(queue, MARU_EVENT_MOUSE_MOVED, MARU_WINDOW_ID_NONE, &move_b);

    push_user_event(queue, MARU_EVENT_USER_1);
    maru_commitQueue(queue);

    struct QueueTrace trace = {0};
    maru_scanQueue(queue, MARU_ALL_EVENTS, on_trace_event, &trace);

    EXPECT_EQ(trace.count, (uint32_t)3);
    EXPECT_EQ(trace.types[0], (MARU_EventId)MARU_EVENT_USER_0);
    EXPECT_EQ(trace.types[1], (MARU_EventId)MARU_EVENT_MOUSE_MOVED);
    EXPECT_EQ(trace.types[2], (MARU_EventId)MARU_EVENT_USER_1);

    EXPECT_EQ(trace.events[1].mouse_moved.dip_position.x, (MARU_Scalar)20.0);
    EXPECT_EQ(trace.events[1].mouse_moved.dip_position.y, (MARU_Scalar)30.0);
    EXPECT_EQ(trace.events[1].mouse_moved.dip_delta.x, (MARU_Scalar)6.0);
    EXPECT_EQ(trace.events[1].mouse_moved.dip_delta.y, (MARU_Scalar)9.0);
    EXPECT_EQ(trace.events[1].mouse_moved.raw_dip_delta.x, (MARU_Scalar)14.0);
    EXPECT_EQ(trace.events[1].mouse_moved.raw_dip_delta.y, (MARU_Scalar)17.0);

    maru_destroyQueue(queue);
}

UTEST(QueueTest, OverflowDropsWhenCompactionCannotFreeSpace) {
    MARU_Queue *queue = NULL;
    EXPECT_EQ(create_queue(2, &queue), (MARU_Status)MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);
    maru_setQueueCoalesceMask(queue, MARU_MASK_MOUSE_MOVED);

    EXPECT_EQ(push_user_event(queue, MARU_EVENT_USER_0),
              (MARU_Status)MARU_SUCCESS);
    EXPECT_EQ(push_user_event(queue, MARU_EVENT_USER_1),
              (MARU_Status)MARU_SUCCESS);
    EXPECT_EQ(push_user_event(queue, MARU_EVENT_USER_2),
              (MARU_Status)MARU_FAILURE);
    maru_commitQueue(queue);

    struct QueueTrace trace = {0};
    maru_scanQueue(queue, MARU_ALL_EVENTS, on_trace_event, &trace);

    EXPECT_EQ(trace.count, (uint32_t)2);
    EXPECT_EQ(trace.types[0], (MARU_EventId)MARU_EVENT_USER_0);
    EXPECT_EQ(trace.types[1], (MARU_EventId)MARU_EVENT_USER_1);

    maru_destroyQueue(queue);
}

struct QueueAllocatorStats {
    uint32_t alloc_count;
    uint32_t free_count;
};

static void *queue_test_alloc(size_t size, void *userdata) {
    struct QueueAllocatorStats *stats = (struct QueueAllocatorStats *)userdata;
    stats->alloc_count++;
    return malloc(size);
}

static void *queue_test_realloc(void *ptr, size_t new_size, void *userdata) {
    (void)userdata;
    return realloc(ptr, new_size);
}

static void queue_test_free(void *ptr, void *userdata) {
    struct QueueAllocatorStats *stats = (struct QueueAllocatorStats *)userdata;
    stats->free_count++;
    free(ptr);
}

UTEST(QueueTest, UsesCustomAllocator) {
    struct QueueAllocatorStats stats = {0};
    MARU_QueueCreateInfo create_info = MARU_QUEUE_CREATE_INFO_DEFAULT;
    create_info.capacity = 8;
    create_info.allocator.alloc_cb = queue_test_alloc;
    create_info.allocator.realloc_cb = queue_test_realloc;
    create_info.allocator.free_cb = queue_test_free;
    create_info.allocator.userdata = &stats;

    MARU_Queue *queue = NULL;
    EXPECT_EQ(maru_createQueue(&create_info, &queue), (MARU_Status)MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);
    EXPECT_EQ(stats.alloc_count, (uint32_t)3);
    EXPECT_EQ(stats.free_count, (uint32_t)0);

    maru_destroyQueue(queue);
    EXPECT_EQ(stats.free_count, (uint32_t)3);
}

UTEST(QueueTest, RejectsPartialAllocator) {
#ifdef MARU_VALIDATE_API_CALLS
    return;
#else
    struct QueueAllocatorStats stats = {0};
    MARU_QueueCreateInfo create_info = MARU_QUEUE_CREATE_INFO_DEFAULT;
    create_info.capacity = 8;
    create_info.allocator.alloc_cb = queue_test_alloc;
    create_info.allocator.userdata = &stats;

    MARU_Queue *queue = NULL;
    EXPECT_EQ(maru_createQueue(&create_info, &queue), (MARU_Status)MARU_FAILURE);
    EXPECT_TRUE(queue == NULL);
    EXPECT_EQ(stats.alloc_count, (uint32_t)0);
    EXPECT_EQ(stats.free_count, (uint32_t)0);
#endif
}

UTEST(QueueTest, SurvivesUnrelatedContextDestruction) {
    MARU_Queue *queue = NULL;
    EXPECT_EQ(create_queue(16, &queue), (MARU_Status)MARU_SUCCESS);
    ASSERT_TRUE(queue != NULL);

    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context *context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);
    EXPECT_EQ(maru_destroyContext(context), (MARU_Status)MARU_SUCCESS);

    EXPECT_EQ(push_user_event(queue, MARU_EVENT_USER_0), (MARU_Status)MARU_SUCCESS);
    EXPECT_EQ(maru_commitQueue(queue), (MARU_Status)MARU_SUCCESS);

    struct QueueTestState state = {0};
    maru_scanQueue(queue, MARU_ALL_EVENTS, on_queue_event, &state);
    EXPECT_EQ(state.event_count, 1);
    EXPECT_EQ(state.last_type, (MARU_EventId)MARU_EVENT_USER_0);

    maru_destroyQueue(queue);
}
