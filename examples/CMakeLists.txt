set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/imgui)

# Create an imgui target since it doesn't provide one
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan)

find_package(Vulkan REQUIRED)

add_library(maru_shared_renderer STATIC
    shared_renderer/vulkan_renderer.c
    shared_renderer/shaders.c
)
target_link_libraries(maru_shared_renderer PUBLIC maru::maru Vulkan::Vulkan)
target_include_directories(maru_shared_renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/shared_renderer)

# Add each example subfolder
add_subdirectory(basic_c)
add_subdirectory(basic_cpp)
add_subdirectory(testbed)
