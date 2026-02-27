#ifndef MARU_TEST_SESSION_H_INCLUDED
#define MARU_TEST_SESSION_H_INCLUDED

#include <stdbool.h>
#include <stddef.h>

#include "automation_driver.h"
#include "tracking_allocator.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MARU_TestSession {
  MARU_Context *context;
  MARU_Window *window;
  MARU_BackendType backend;
  const MARU_TestAutomationDriver *driver;
  bool active;
  MARU_IntegrationTrackingAllocator tracking_allocator;
} MARU_TestSession;

bool maru_test_session_create(MARU_TestSession *out_session,
                              MARU_BackendType requested_backend, char *reason,
                              size_t reason_capacity);
void maru_test_session_destroy(MARU_TestSession *session);

#ifdef __cplusplus
}
#endif

#endif  // MARU_TEST_SESSION_H_INCLUDED
