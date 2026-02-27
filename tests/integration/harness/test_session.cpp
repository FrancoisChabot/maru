#include "test_session.h"

#include <stdio.h>

#include "maru/c/details/contexts.h"

static void _maru_test_set_reason(char *reason, size_t reason_capacity,
                                  const char *message) {
  if (!reason || reason_capacity == 0) {
    return;
  }
  snprintf(reason, reason_capacity, "%s", message ? message : "");
}

void maru_test_session_destroy(MARU_TestSession *session) {
  if (!session) {
    return;
  }

  if (session->driver) {
    session->driver->stop_session(session);
  }
  if (session->window) {
    (void)maru_destroyWindow(session->window);
    session->window = NULL;
  }
  if (session->context) {
    (void)maru_destroyContext(session->context);
    session->context = NULL;
  }
  session->tracking_allocator.reset();
  session->backend = MARU_BACKEND_UNKNOWN;
  session->driver = NULL;
  session->active = false;
}

bool maru_test_session_create(MARU_TestSession *out_session,
                              MARU_BackendType requested_backend, char *reason,
                              size_t reason_capacity) {
  if (!out_session) {
    _maru_test_set_reason(reason, reason_capacity, "Output session is NULL.");
    return false;
  }
  out_session->context = NULL;
  out_session->window = NULL;
  out_session->backend = MARU_BACKEND_UNKNOWN;
  out_session->driver = NULL;
  out_session->active = false;
  out_session->tracking_allocator.reset();

  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  out_session->tracking_allocator.apply(&create_info);
  create_info.backend = requested_backend;
  if (maru_createContext(&create_info, &out_session->context) != MARU_SUCCESS) {
    _maru_test_set_reason(reason, reason_capacity,
                          "Failed to create Maru context.");
    maru_test_session_destroy(out_session);
    return false;
  }

  out_session->backend = maru_getContextBackend(out_session->context);
  out_session->driver = maru_test_get_automation_driver(out_session->backend);
  if (!out_session->driver) {
    _maru_test_set_reason(reason, reason_capacity,
                          "No automation driver for selected backend.");
    maru_test_session_destroy(out_session);
    return false;
  }

  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru Desktop Integration Test";
  if (maru_createWindow(out_session->context, &window_info,
                        &out_session->window) != MARU_SUCCESS) {
    _maru_test_set_reason(reason, reason_capacity,
                          "Failed to create Maru window.");
    maru_test_session_destroy(out_session);
    return false;
  }

  if (!out_session->driver->start_session(out_session, reason, reason_capacity)) {
    maru_test_session_destroy(out_session);
    return false;
  }

  if (!out_session->driver->ensure_presented(out_session, out_session->window,
                                             reason, reason_capacity)) {
    maru_test_session_destroy(out_session);
    return false;
  }

  out_session->active = true;
  _maru_test_set_reason(reason, reason_capacity, "");
  return true;
}
