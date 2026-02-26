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

# utest.h doesn't need a discovery mechanism like GTest, it just runs.
# But we can still register it as a test.
add_test(NAME maru_tests COMMAND maru_tests)
