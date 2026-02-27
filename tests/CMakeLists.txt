add_executable(maru_tests
  test_allocator.c
  test_diagnostics.c
)

if (UNIX AND NOT APPLE)
  target_sources(maru_tests PRIVATE test_linux_worker.c)
endif()

target_include_directories(maru_tests PRIVATE ${PROJECT_SOURCE_DIR}/src/core)

target_link_libraries(maru_tests
  PRIVATE
    maru::maru
    ${MARU_ACTIVE_CORE_IFACE}
)

# find_package(GTest CONFIG REQUIRED)

# set(MARU_INTEGRATION_SOURCES
#   integration/harness/automation_driver.cpp
#   integration/harness/test_session.cpp
#   integration/harness/tracking_allocator.cpp
#   integration/test_context.cpp
#   integration/test_desktop_smoke.cpp
#   integration/test_window.cpp
# )

# if (UNIX AND NOT APPLE)
#   list(APPEND MARU_INTEGRATION_SOURCES
#     integration/harness/adapters/linux_wayland_driver.cpp
#     integration/harness/adapters/linux_x11_driver.cpp
#   )
# elseif (WIN32)
#   list(APPEND MARU_INTEGRATION_SOURCES
#     integration/harness/adapters/windows_driver.cpp
#   )
# elseif (APPLE)
#   list(APPEND MARU_INTEGRATION_SOURCES
#     integration/harness/adapters/macos_driver.cpp
#   )
# endif()

# add_executable(maru_integration_tests
#   ${MARU_INTEGRATION_SOURCES}
# )

# target_include_directories(maru_integration_tests PRIVATE
#   ${PROJECT_SOURCE_DIR}/src/core
#   ${CMAKE_CURRENT_SOURCE_DIR}
# )

# target_link_libraries(maru_integration_tests
#   PRIVATE
#     maru::maru
#     ${MARU_ACTIVE_CORE_IFACE}
#     GTest::gtest_main
# )

# set_property(TARGET maru_integration_tests PROPERTY CXX_STANDARD 17)
# set_property(TARGET maru_integration_tests PROPERTY CXX_STANDARD_REQUIRED ON)

# add_test(NAME maru_tests COMMAND maru_tests)
# set_tests_properties(maru_tests PROPERTIES LABELS "core")

# add_test(NAME maru_integration_tests COMMAND maru_integration_tests)
# set_tests_properties(maru_integration_tests PROPERTIES LABELS "desktop_integration")
