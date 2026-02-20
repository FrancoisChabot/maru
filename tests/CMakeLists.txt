include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.17.0
)
# For Windows: Do not override the settings for the side-by-side static runtime
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(maru_tests
  test_allocator.cpp
  test_diagnostics.cpp
)

target_compile_features(maru_tests PRIVATE cxx_std_20)

target_link_libraries(maru_tests
  PRIVATE
    maru::maru
    maru_core_iface_d
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(maru_tests)
