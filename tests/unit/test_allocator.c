#include "utest.h"
#include "maru/maru.h"
#include "maru_test_utils.h"

struct CustomAllocatorState {
    int alloc_count;
    int realloc_count;
    int free_count;
    size_t last_alloc_size;
    size_t last_realloc_size;
};

static void* custom_alloc(size_t size, void* userdata) {
    struct CustomAllocatorState* state = (struct CustomAllocatorState*)userdata;
    state->alloc_count++;
    state->last_alloc_size = size;
    return malloc(size);
}

static void custom_free(void* ptr, void* userdata) {
    struct CustomAllocatorState* state = (struct CustomAllocatorState*)userdata;
    if (ptr) {
        state->free_count++;
        free(ptr);
    }
}

static void* custom_realloc(void* ptr, size_t size, void* userdata) {
    struct CustomAllocatorState* state = (struct CustomAllocatorState*)userdata;
    state->realloc_count++;
    state->last_realloc_size = size;
    return realloc(ptr, size);
}

UTEST(AllocatorTest, CustomAllocatorIsUsed) {
    struct CustomAllocatorState state = {0};
    
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    create_info.allocator.alloc_cb = custom_alloc;
    create_info.allocator.realloc_cb = custom_realloc;
    create_info.allocator.free_cb = custom_free;
    create_info.allocator.userdata = &state;
    
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);
    EXPECT_GT(state.alloc_count, 0);
    
    maru_destroyContext(context);
    EXPECT_EQ(state.free_count, state.alloc_count);
}

UTEST(AllocatorTest, DefaultAllocatorWorks) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_TestTrackingAllocator tracking = {0};
    maru_test_tracking_allocator_init(&tracking);
    maru_test_tracking_allocator_apply(&tracking, &create_info);
    
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);
    
    maru_destroyContext(context);
    EXPECT_TRUE(maru_test_tracking_allocator_is_clean(&tracking));
    maru_test_tracking_allocator_shutdown(&tracking);
}

UTEST_MAIN()
