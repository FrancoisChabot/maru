#include "utest.h"
#include "maru/maru.h"
#include "maru/c/instrumentation.h"
#include "maru_test_utils.h"

struct DiagnosticState {
    MARU_Diagnostic last_diag;
    int call_count;
};

static void diagnostic_cb(const MARU_DiagnosticInfo* info, void* userdata) {
    struct DiagnosticState* state = (struct DiagnosticState*)userdata;
    state->last_diag = info->diagnostic;
    state->call_count++;
}

UTEST(DiagnosticTest, UpdateContextAndReport) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_TestTrackingAllocator tracking = {0};
    maru_test_tracking_allocator_init(&tracking);
    maru_test_tracking_allocator_apply(&tracking, &create_info);
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);

    struct DiagnosticState state = {0};
    MARU_ContextAttributes attrs = {0};
    attrs.diagnostic_cb = diagnostic_cb;
    attrs.diagnostic_userdata = &state;

    EXPECT_EQ(maru_updateContext(context, MARU_CONTEXT_ATTR_DIAGNOSTICS, &attrs), MARU_SUCCESS);

    _maru_reportDiagnostic(context, MARU_DIAGNOSTIC_INFO, "Test message");

    EXPECT_EQ(state.call_count, 1);
    EXPECT_EQ(state.last_diag, MARU_DIAGNOSTIC_INFO);

    maru_destroyContext(context);
    EXPECT_TRUE(maru_test_tracking_allocator_is_clean(&tracking));
    maru_test_tracking_allocator_shutdown(&tracking);
}

UTEST(DiagnosticTest, DefaultReportingDoesNotCrash) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_TestTrackingAllocator tracking = {0};
    maru_test_tracking_allocator_init(&tracking);
    maru_test_tracking_allocator_apply(&tracking, &create_info);
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_TRUE(context != NULL);

    // Should not crash even with no callback
    _maru_reportDiagnostic(context, MARU_DIAGNOSTIC_INFO, "Test message");

    maru_destroyContext(context);
    EXPECT_TRUE(maru_test_tracking_allocator_is_clean(&tracking));
    maru_test_tracking_allocator_shutdown(&tracking);
}
