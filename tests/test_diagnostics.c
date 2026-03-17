#include "utest.h"
#include "maru/maru.h"
#include "maru_test_utils.h"

#ifdef MARU_ENABLE_DIAGNOSTICS
extern void _maru_reportDiagnostic(const MARU_Context *ctx, MARU_Diagnostic diag, const char *msg);
#endif

struct DiagnosticState {
    MARU_Diagnostic last_diag;
    MARU_DiagnosticSubjectKind last_subject_kind;
    const MARU_Context* last_context;
    int call_count;
};

static void diagnostic_cb(const MARU_DiagnosticInfo* info, void* userdata) {
    struct DiagnosticState* state = (struct DiagnosticState*)userdata;
    state->last_diag = info->diagnostic;
    state->last_subject_kind = info->subject_kind;
    state->last_context = info->context;
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

    #ifdef MARU_ENABLE_DIAGNOSTICS
    _maru_reportDiagnostic(context, MARU_DIAGNOSTIC_INFO, "Test message");
#endif

#ifdef MARU_ENABLE_DIAGNOSTICS
    EXPECT_EQ(state.call_count, 1);
    EXPECT_EQ(state.last_diag, MARU_DIAGNOSTIC_INFO);
    EXPECT_EQ(state.last_subject_kind, MARU_DIAGNOSTIC_SUBJECT_NONE);
    EXPECT_TRUE(state.last_context == context);
#else
    EXPECT_EQ(state.call_count, 0);
#endif
    
    maru_test_destroyContext(context);
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
    #ifdef MARU_ENABLE_DIAGNOSTICS
    _maru_reportDiagnostic(context, MARU_DIAGNOSTIC_INFO, "Test message");
#endif

    maru_test_destroyContext(context);
    EXPECT_TRUE(maru_test_tracking_allocator_is_clean(&tracking));
    maru_test_tracking_allocator_shutdown(&tracking);
}
