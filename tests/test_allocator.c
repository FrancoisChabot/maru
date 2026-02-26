#include "utest.h"
#include "maru/maru.h"
#include "maru/c/instrumentation.h"
#include "maru_test_utils.h"

struct CustomAllocatorState {
    int alloc_count;
    int free_count;
    size_t last_alloc_size;
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

UTEST(AllocatorTest, CustomAllocatorIsUsed) {
    struct CustomAllocatorState state = {0};
    
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    create_info.allocator.alloc_cb = custom_alloc;
    create_info.allocator.free_cb = custom_free;
    create_info.allocator.userdata = &state;
    
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);
    EXPECT_GT(state.alloc_count, 0);
    
    maru_destroyContext(context);
    EXPECT_EQ(state.free_count, state.alloc_count);
}

UTEST(AllocatorTest, MemoryMetricsTracking) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);
    
    const MARU_ContextMetrics* metrics = maru_getContextMetrics(context);
    ASSERT_TRUE(metrics != NULL);
    
#ifdef MARU_GATHER_METRICS
    uint64_t initial_mem = metrics->memory_allocated_current;
    EXPECT_GT(initial_mem, 0);
    EXPECT_GE(metrics->memory_allocated_peak, initial_mem);
    
    void* ptr = maru_context_alloc((MARU_Context_Base*)context, 1024);
    ASSERT_TRUE(ptr != NULL);
    
    EXPECT_EQ(metrics->memory_allocated_current, initial_mem + 1024);
    EXPECT_GE(metrics->memory_allocated_peak, initial_mem + 1024);
    
    maru_context_free((MARU_Context_Base*)context, ptr);
    EXPECT_EQ(metrics->memory_allocated_current, initial_mem);
#endif
    
    maru_destroyContext(context);
}

UTEST(AllocatorTest, MemoryMetricsPeakReset) {
#ifdef MARU_GATHER_METRICS
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);
    
    const MARU_ContextMetrics* metrics = maru_getContextMetrics(context);
    
    void* ptr = maru_context_alloc((MARU_Context_Base*)context, 2048);
    uint64_t peak_with_alloc = metrics->memory_allocated_peak;
    
    maru_context_free((MARU_Context_Base*)context, ptr);
    
    EXPECT_EQ(metrics->memory_allocated_peak, peak_with_alloc);
    
    maru_resetContextMetrics(context);
    
    // Peak should be reset to current
    EXPECT_EQ(metrics->memory_allocated_peak, metrics->memory_allocated_current);
    EXPECT_LT(metrics->memory_allocated_peak, peak_with_alloc);
    
    maru_destroyContext(context);
#endif
}

UTEST(AllocatorTest, DefaultAllocatorWorks) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);
    
    EXPECT_EQ(maru_destroyContext(context), MARU_SUCCESS);
}

UTEST_MAIN()

