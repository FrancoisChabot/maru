#ifndef MARU_TEST_AUTOMATION_DRIVER_H_INCLUDED
#define MARU_TEST_AUTOMATION_DRIVER_H_INCLUDED

#include <cstddef>

#include "maru/maru.h"

class MARU_TestSession;

class MARU_TestAutomationDriver {
 public:
  virtual ~MARU_TestAutomationDriver() = default;

  virtual const char *name() const = 0;
  virtual bool start_session(MARU_TestSession *session, char *reason,
                             size_t reason_capacity) const = 0;
  virtual void stop_session(MARU_TestSession *session) const = 0;
  virtual bool ensure_presented(MARU_TestSession *session, MARU_Window *window,
                                char *reason, size_t reason_capacity) const = 0;
  virtual bool inject_close_request(MARU_TestSession *session,
                                    MARU_Window *window, char *reason,
                                    size_t reason_capacity) const = 0;
  virtual bool inject_noop(MARU_TestSession *session, char *reason,
                           size_t reason_capacity) const = 0;
};

const MARU_TestAutomationDriver *
maru_test_get_automation_driver(MARU_BackendType backend);

#endif  // MARU_TEST_AUTOMATION_DRIVER_H_INCLUDED
