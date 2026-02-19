# Standard double precision examples
add_executable(maru_basic_c basic_c/main.c)
target_link_libraries(maru_basic_c PRIVATE maru::maru)

add_executable(maru_testbed testbed/main.c)
target_link_libraries(maru_testbed PRIVATE maru::maru)

# Float precision examples
add_executable(maru_basic_c_f basic_c/main.c)
target_link_libraries(maru_basic_c_f PRIVATE maru::maru_f)
target_compile_definitions(maru_basic_c_f PRIVATE MARU_USE_FLOAT)

add_executable(maru_testbed_f testbed/main.c)
target_link_libraries(maru_testbed_f PRIVATE maru::maru_f)
target_compile_definitions(maru_testbed_f PRIVATE MARU_USE_FLOAT)
