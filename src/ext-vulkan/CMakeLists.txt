set(MARU_VULKAN_LINUX_SOURCES linux.c)
set(MARU_VULKAN_WAYLAND_SOURCES wayland.c)
set(MARU_VULKAN_X11_SOURCES x11.c)

macro(define_vulkan_flavor_targets suffix)
    set(core_iface "maru_core_iface${suffix}")

    if (MARU_ENABLE_VULKAN)
        # Indirect dispatch (for maru_linux)
        add_library("maru_vulkan_linux${suffix}_obj" OBJECT ${MARU_VULKAN_LINUX_SOURCES})
        target_link_libraries("maru_vulkan_linux${suffix}_obj" PRIVATE ${core_iface})
        target_compile_definitions("maru_vulkan_linux${suffix}_obj" PRIVATE MARU_INDIRECT_BACKEND)
        set_property(TARGET "maru_vulkan_linux${suffix}_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

        # Wayland implementation
        add_library("maru_vulkan_wayland${suffix}_obj" OBJECT ${MARU_VULKAN_WAYLAND_SOURCES})
        target_link_libraries("maru_vulkan_wayland${suffix}_obj" PRIVATE ${core_iface})
        set_property(TARGET "maru_vulkan_wayland${suffix}_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

        add_library("maru_vulkan_wayland${suffix}_indirect_obj" OBJECT ${MARU_VULKAN_WAYLAND_SOURCES})
        target_link_libraries("maru_vulkan_wayland${suffix}_indirect_obj" PRIVATE ${core_iface})
        target_compile_definitions("maru_vulkan_wayland${suffix}_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
        set_property(TARGET "maru_vulkan_wayland${suffix}_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

        # X11 implementation
        add_library("maru_vulkan_x11${suffix}_obj" OBJECT ${MARU_VULKAN_X11_SOURCES})
        target_link_libraries("maru_vulkan_x11${suffix}_obj" PRIVATE ${core_iface})
        set_property(TARGET "maru_vulkan_x11${suffix}_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

        add_library("maru_vulkan_x11${suffix}_indirect_obj" OBJECT ${MARU_VULKAN_X11_SOURCES})
        target_link_libraries("maru_vulkan_x11${suffix}_indirect_obj" PRIVATE ${core_iface})
        target_compile_definitions("maru_vulkan_x11${suffix}_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
        set_property(TARGET "maru_vulkan_x11${suffix}_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})
    endif()
endmacro()

define_vulkan_flavor_targets("_d")
define_vulkan_flavor_targets("_f")
