set(MARU_VULKAN_LINUX_SOURCES linux.c)
set(MARU_VULKAN_WAYLAND_SOURCES wayland.c)
set(MARU_VULKAN_X11_SOURCES x11.c)
set(MARU_VULKAN_WINDOWS_SOURCES windows.c)

if (MARU_ENABLE_VULKAN)
    if (WIN32)
        # Windows implementation
        add_library("maru_vulkan_windows_obj" OBJECT ${MARU_VULKAN_WINDOWS_SOURCES})
        target_link_libraries("maru_vulkan_windows_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
        set_property(TARGET "maru_vulkan_windows_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

        add_library("maru_vulkan_windows_indirect_obj" OBJECT ${MARU_VULKAN_WINDOWS_SOURCES})
        target_link_libraries("maru_vulkan_windows_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
        target_compile_definitions("maru_vulkan_windows_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
        set_property(TARGET "maru_vulkan_windows_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})
    elseif (NOT APPLE)
        # Indirect dispatch (for maru_linux)
        add_library("maru_vulkan_linux_obj" OBJECT ${MARU_VULKAN_LINUX_SOURCES})
        target_link_libraries("maru_vulkan_linux_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
        target_compile_definitions("maru_vulkan_linux_obj" PRIVATE MARU_INDIRECT_BACKEND)
        set_property(TARGET "maru_vulkan_linux_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

        # Wayland implementation
        add_library("maru_vulkan_wayland_obj" OBJECT ${MARU_VULKAN_WAYLAND_SOURCES})
        target_link_libraries("maru_vulkan_wayland_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE} maru_wayland_internal_iface)
        set_property(TARGET "maru_vulkan_wayland_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

        add_library("maru_vulkan_wayland_indirect_obj" OBJECT ${MARU_VULKAN_WAYLAND_SOURCES})
        target_link_libraries("maru_vulkan_wayland_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE} maru_wayland_internal_iface)
        target_compile_definitions("maru_vulkan_wayland_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
        set_property(TARGET "maru_vulkan_wayland_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

        # X11 implementation
        add_library("maru_vulkan_x11_obj" OBJECT ${MARU_VULKAN_X11_SOURCES})
        target_link_libraries("maru_vulkan_x11_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
        set_property(TARGET "maru_vulkan_x11_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

        add_library("maru_vulkan_x11_indirect_obj" OBJECT ${MARU_VULKAN_X11_SOURCES})
        target_link_libraries("maru_vulkan_x11_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
        target_compile_definitions("maru_vulkan_x11_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
        set_property(TARGET "maru_vulkan_x11_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})
    endif()
endif()
