find_package(Vulkan REQUIRED)

add_library(maru_shared_renderer STATIC
    shared_renderer/vulkan_renderer.c
    shared_renderer/shaders.c
)
target_link_libraries(maru_shared_renderer PUBLIC maru::maru Vulkan::Vulkan)
target_include_directories(maru_shared_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared_renderer)

# Standard double precision examples
add_executable(maru_basic_c basic_c/main.c)
target_link_libraries(maru_basic_c PRIVATE maru::maru maru_shared_renderer)

add_executable(maru_testbed testbed/main.c)
target_link_libraries(maru_testbed PRIVATE maru::maru)

# Float precision examples
add_executable(maru_basic_c_f basic_c/main.c)
target_link_libraries(maru_basic_c_f PRIVATE maru::maru_f maru_shared_renderer)
target_compile_definitions(maru_basic_c_f PRIVATE MARU_USE_FLOAT)

add_executable(maru_testbed_f testbed/main.c)
target_link_libraries(maru_testbed_f PRIVATE maru::maru_f)
target_compile_definitions(maru_testbed_f PRIVATE MARU_USE_FLOAT)
