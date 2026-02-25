include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.8
)
FetchContent_MakeAvailable(imgui)

# Patch ImGui Vulkan helper to allow compositor alpha for Wayland transparency.
# Upstream defaults to VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, which forces opaque composition.
set(IMGUI_VULKAN_BACKEND "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp")
file(READ "${IMGUI_VULKAN_BACKEND}" IMGUI_VULKAN_BACKEND_SRC)
set(IMGUI_VULKAN_OLD_LINE "        info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;")
set(IMGUI_VULKAN_NEW_BLOCK
"        VkCompositeAlphaFlagBitsKHR composite_alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        if (cap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
            composite_alpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        else if (cap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
            composite_alpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        else if (cap.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
            composite_alpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        info.compositeAlpha = composite_alpha;")
string(FIND "${IMGUI_VULKAN_BACKEND_SRC}" "${IMGUI_VULKAN_OLD_LINE}" IMGUI_VULKAN_PATCH_POS)
if (IMGUI_VULKAN_PATCH_POS GREATER -1)
    string(REPLACE "${IMGUI_VULKAN_OLD_LINE}" "${IMGUI_VULKAN_NEW_BLOCK}" IMGUI_VULKAN_BACKEND_SRC "${IMGUI_VULKAN_BACKEND_SRC}")
    file(WRITE "${IMGUI_VULKAN_BACKEND}" "${IMGUI_VULKAN_BACKEND_SRC}")
endif()

# Create an imgui target since it doesn't provide one
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)
target_include_directories(imgui PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC Vulkan::Vulkan)

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

add_executable(maru_testbed 
    testbed/main.cpp 
    testbed/imgui_impl_maru.cpp
    testbed/app.cpp
    testbed/modules/event_log_module.cpp
    testbed/modules/event_module.cpp
    testbed/modules/metrics_module.cpp
    testbed/modules/monitor_module.cpp
    testbed/modules/cursor_module.cpp
    testbed/modules/window_module.cpp
    testbed/modules/input_module.cpp
    testbed/modules/instrumentation_module.cpp
    testbed/modules/composition_module.cpp
)
target_link_libraries(maru_testbed PRIVATE maru::maru imgui)
set_target_properties(maru_testbed PROPERTIES CXX_STANDARD 20)
