#include "../automation_driver.h"

#include <cstdio>

namespace {

void set_reason(char *reason, size_t reason_capacity,
                const char *message) {
  if (!reason || reason_capacity == 0) {
    return;
  }
  std::snprintf(reason, reason_capacity, "%s", message ? message : "");
}

class WindowsDriver : public MARU_TestAutomationDriver {
 public:
  const char *name() const override { return "windows"; }
  bool start_session(MARU_TestSession *, char *reason,
                     size_t reason_capacity) const override {
    set_reason(reason, reason_capacity, "");
    return true;
  }
  void stop_session(MARU_TestSession *) const override {}
  bool ensure_presented(MARU_TestSession *, MARU_Window *, char *reason,
                        size_t reason_capacity) const override {
    set_reason(reason, reason_capacity, "");
    return true;
  }
  bool inject_close_request(MARU_TestSession *, MARU_Window *, char *reason,
                            size_t reason_capacity) const override {
    set_reason(reason, reason_capacity,
               "Close request injection is not implemented for this driver.");
    return false;
  }
  bool inject_noop(MARU_TestSession *, char *reason,
                   size_t reason_capacity) const override {
    set_reason(reason, reason_capacity,
               "Windows automation injection is not implemented yet in this "
               "harness.");
    return false;
  }
};

}  // namespace

const MARU_TestAutomationDriver *maru_test_windows_driver() {
  static WindowsDriver driver;
  return &driver;
}
