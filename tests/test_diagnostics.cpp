/*
#include <gtest/gtest.h>
#include "maru/maru.h"
#include "maru/c/instrumentation.h"
#include "maru_test_utils.h"

struct DiagnosticState {
    MARU_Diagnostic last_diag = MARU_DIAGNOSTIC_NONE;
    int call_count = 0;
};

static void diagnostic_cb(const MARU_DiagnosticInfo* info, void* userdata) {
    auto* state = static_cast<DiagnosticState*>(userdata);
    state->last_diag = info->diagnostic;
    state->call_count++;
}

TEST(DiagnosticTest, UpdateContextAndReport) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_NE(context, nullptr);

    DiagnosticState state;
    MARU_ContextAttributes attrs = {};
    attrs.diagnostic_cb = diagnostic_cb;
    attrs.diagnostic_userdata = &state;

    EXPECT_EQ(maru_updateContext(context, MARU_CONTEXT_ATTR_DIAGNOSTICS, &attrs), MARU_SUCCESS);

    _maru_reportDiagnostic(context, MARU_DIAGNOSTIC_INFO, "Test message");

    EXPECT_EQ(state.call_count, 1);
    EXPECT_EQ(state.last_diag, MARU_DIAGNOSTIC_INFO);

    maru_destroyContext(context);
}

TEST(DiagnosticTest, DefaultReportingDoesNotCrash) {
    MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context* context = maru_test_createContext(&create_info);
    ASSERT_NE(context, nullptr);

    // Should not crash even with no callback
    _maru_reportDiagnostic(context, MARU_DIAGNOSTIC_INFO, "Test message");

    maru_destroyContext(context);
}
*/