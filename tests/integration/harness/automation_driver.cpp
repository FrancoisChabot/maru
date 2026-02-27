#include "automation_driver.h"

#if defined(__linux__) && !defined(__APPLE__)
const MARU_TestAutomationDriver *maru_test_linux_wayland_driver(void);
const MARU_TestAutomationDriver *maru_test_linux_x11_driver(void);
#elif defined(_WIN32)
const MARU_TestAutomationDriver *maru_test_windows_driver(void);
#elif defined(__APPLE__)
const MARU_TestAutomationDriver *maru_test_macos_driver(void);
#endif

const MARU_TestAutomationDriver *
maru_test_get_automation_driver(MARU_BackendType backend) {
  switch (backend) {
#if defined(__linux__) && !defined(__APPLE__)
    case MARU_BACKEND_WAYLAND:
      return maru_test_linux_wayland_driver();
    case MARU_BACKEND_X11:
      return maru_test_linux_x11_driver();
#elif defined(_WIN32)
    case MARU_BACKEND_WINDOWS:
      return maru_test_windows_driver();
#elif defined(__APPLE__)
    case MARU_BACKEND_COCOA:
      return maru_test_macos_driver();
#endif
    default:
      return nullptr;
  }
}
