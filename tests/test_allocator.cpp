// #include <gtest/gtest.h>
// #include "maru/maru.h"
// #include "maru/c/instrumentation.h"
// #include "maru_test_utils.h"

// struct CustomAllocatorState {
//     int alloc_count = 0;
//     int free_count = 0;
//     size_t last_alloc_size = 0;
// };

// static void* custom_alloc(size_t size, void* userdata) {
//     auto* state = static_cast<CustomAllocatorState*>(userdata);
//     state->alloc_count++;
//     state->last_alloc_size = size;
//     return malloc(size);
// }

// static void custom_free(void* ptr, void* userdata) {
//     auto* state = static_cast<CustomAllocatorState*>(userdata);
//     if (ptr) {
//         state->free_count++;
//         free(ptr);
//     }
// }

// TEST(AllocatorTest, CustomAllocatorIsUsed) {
//     CustomAllocatorState state;
    
//     MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
//     create_info.allocator.alloc_cb = custom_alloc;
//     create_info.allocator.free_cb = custom_free;
//     create_info.allocator.userdata = &state;
    
//     MARU_Context* context = maru_test_createContext(&create_info);
//     ASSERT_NE(context, nullptr);
//     EXPECT_GT(state.alloc_count, 0);
    
//     maru_destroyContext(context);
//     EXPECT_EQ(state.free_count, state.alloc_count);
// }

// TEST(AllocatorTest, DefaultAllocatorWorks) {
//     MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    
//     MARU_Context* context = maru_test_createContext(&create_info);
//     ASSERT_NE(context, nullptr);
    
//     EXPECT_EQ(maru_destroyContext(context), MARU_SUCCESS);
// }


