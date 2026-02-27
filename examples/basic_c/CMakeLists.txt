add_executable(maru_basic_c main.c)
target_link_libraries(maru_basic_c PRIVATE maru::maru maru_shared_renderer)
