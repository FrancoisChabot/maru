add_executable(maru_tests
  unit/test_allocator.c
  unit/test_diagnostics.c
)

if (UNIX AND NOT APPLE)
  target_sources(maru_tests PRIVATE unit/test_linux_worker.c)
endif()

target_include_directories(maru_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/src/core
  ${CMAKE_CURRENT_SOURCE_DIR}/unit
)

target_link_libraries(maru_tests
  PRIVATE
    maru::maru
    ${MARU_ACTIVE_CORE_IFACE}
)

add_test(NAME maru_tests COMMAND maru_tests)

add_executable(maru_integration_tests
  integration/doctest_main.cpp
  integration/test_context.cpp
  integration/test_desktop_smoke.cpp
  integration/test_window.cpp
  integration/support/tracking_allocator.cpp
)

target_include_directories(maru_integration_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/src/core
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/vendor
)

target_link_libraries(maru_integration_tests
  PRIVATE
    maru::maru
    ${MARU_ACTIVE_CORE_IFACE}
)

set_property(TARGET maru_integration_tests PROPERTY CXX_STANDARD 17)
set_property(TARGET maru_integration_tests PROPERTY CXX_STANDARD_REQUIRED ON)

add_test(NAME maru_integration_tests COMMAND maru_integration_tests)
set_tests_properties(maru_integration_tests PROPERTIES LABELS "desktop_integration")
