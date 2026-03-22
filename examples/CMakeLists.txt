set(EXAMPLES_SUPPORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/support)
set(IMGUI_DIR ${EXAMPLES_SUPPORT_DIR}/vendor/imgui)

# Create an imgui target since it doesn't provide one
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui SYSTEM PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan)

find_package(Vulkan REQUIRED)

add_library(maru_shared_renderer STATIC
    support/shared_renderer/vulkan_renderer.c
    support/shared_renderer/shaders.c
)
target_link_libraries(maru_shared_renderer PUBLIC maru::maru Vulkan::Vulkan PRIVATE maru_common_settings)
target_include_directories(maru_shared_renderer PUBLIC ${EXAMPLES_SUPPORT_DIR}/shared_renderer)

# We want examples to have the same warning levels as the library itself
set(example_settings maru_common_settings)

# Add each example subfolder
add_subdirectory(basic_c)
add_subdirectory(basic_cpp)
add_subdirectory(queue_cpp)
add_subdirectory(testbed)
add_subdirectory(frame_sync_c)

if(APPLE)
    add_subdirectory(basic_metal)
endif()
