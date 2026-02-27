add_executable(maru_basic_cpp main.cpp)
target_compile_features(maru_basic_cpp PRIVATE cxx_std_20)
target_link_libraries(maru_basic_cpp PRIVATE maru::maru maru_shared_renderer)
