set(MARU_VULKAN_LINUX_SOURCES linux.c)
set(MARU_VULKAN_WAYLAND_SOURCES wayland.c)
set(MARU_VULKAN_X11_SOURCES x11.c)
set(MARU_VULKAN_WINDOWS_SOURCES windows.c)

if (WIN32)
    # --- Windows Direct ---
    add_library("maru_vulkan_windows${MARU_LIB_SUFFIX}" ${MARU_LIB_TYPE} ${MARU_VULKAN_WINDOWS_SOURCES})
    target_link_libraries("maru_vulkan_windows${MARU_LIB_SUFFIX}" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
    target_link_libraries("maru_vulkan_windows${MARU_LIB_SUFFIX}" PUBLIC maru_iface${MARU_LIB_SUFFIX}_direct)
    set_property(TARGET "maru_vulkan_windows${MARU_LIB_SUFFIX}" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    # --- Windows Indirect ---
    add_library("maru_vulkan_windows_indirect${MARU_LIB_SUFFIX}" ${MARU_LIB_TYPE} ${MARU_VULKAN_WINDOWS_SOURCES})
    target_link_libraries("maru_vulkan_windows_indirect${MARU_LIB_SUFFIX}" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
    target_link_libraries("maru_vulkan_windows_indirect${MARU_LIB_SUFFIX}" PUBLIC maru_iface${MARU_LIB_SUFFIX}_indirect)
    target_compile_definitions("maru_vulkan_windows_indirect${MARU_LIB_SUFFIX}" PRIVATE MARU_INDIRECT_BACKEND)
    set_property(TARGET "maru_vulkan_windows_indirect${MARU_LIB_SUFFIX}" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    list(APPEND MARU_INSTALL_TARGETS "maru_vulkan_windows${MARU_LIB_SUFFIX}" "maru_vulkan_windows_indirect${MARU_LIB_SUFFIX}")
    set_target_properties("maru_vulkan_windows${MARU_LIB_SUFFIX}" PROPERTIES EXPORT_NAME vulkan${MARU_LIB_SUFFIX})
    set_target_properties("maru_vulkan_windows_indirect${MARU_LIB_SUFFIX}" PROPERTIES EXPORT_NAME vulkan_indirect${MARU_LIB_SUFFIX})

elseif (NOT APPLE)
    # --- Wayland Direct ---
    add_library("maru_vulkan_wayland${MARU_LIB_SUFFIX}" ${MARU_LIB_TYPE} ${MARU_VULKAN_WAYLAND_SOURCES})
    target_link_libraries("maru_vulkan_wayland${MARU_LIB_SUFFIX}" PRIVATE ${MARU_ACTIVE_CORE_IFACE} maru_wayland_internal_iface)
    target_link_libraries("maru_vulkan_wayland${MARU_LIB_SUFFIX}" PUBLIC maru_iface${MARU_LIB_SUFFIX}_direct)
    set_property(TARGET "maru_vulkan_wayland${MARU_LIB_SUFFIX}" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    # --- X11 Direct ---
    add_library("maru_vulkan_x11${MARU_LIB_SUFFIX}" ${MARU_LIB_TYPE} ${MARU_VULKAN_X11_SOURCES})
    target_link_libraries("maru_vulkan_x11${MARU_LIB_SUFFIX}" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
    target_link_libraries("maru_vulkan_x11${MARU_LIB_SUFFIX}" PUBLIC maru_iface${MARU_LIB_SUFFIX}_direct)
    set_property(TARGET "maru_vulkan_x11${MARU_LIB_SUFFIX}" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    # --- Linux Indirect (The Dispatcher) ---
    # We need to bundle the indirect implementations of Wayland and X11 into the Linux indirect library
    add_library("maru_vulkan_wayland_indirect_obj" OBJECT ${MARU_VULKAN_WAYLAND_SOURCES})
    target_link_libraries("maru_vulkan_wayland_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE} maru_wayland_internal_iface)
    target_compile_definitions("maru_vulkan_wayland_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
    set_property(TARGET "maru_vulkan_wayland_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    add_library("maru_vulkan_x11_indirect_obj" OBJECT ${MARU_VULKAN_X11_SOURCES})
    target_link_libraries("maru_vulkan_x11_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
    target_compile_definitions("maru_vulkan_x11_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
    set_property(TARGET "maru_vulkan_x11_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    add_library("maru_vulkan_linux_indirect_obj" OBJECT ${MARU_VULKAN_LINUX_SOURCES})
    target_link_libraries("maru_vulkan_linux_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
    target_compile_definitions("maru_vulkan_linux_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
    set_property(TARGET "maru_vulkan_linux_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    add_library("maru_vulkan_linux${MARU_LIB_SUFFIX}" ${MARU_LIB_TYPE} 
        $<TARGET_OBJECTS:maru_vulkan_linux_indirect_obj>
        $<TARGET_OBJECTS:maru_vulkan_wayland_indirect_obj>
        $<TARGET_OBJECTS:maru_vulkan_x11_indirect_obj>
    )
    target_link_libraries("maru_vulkan_linux${MARU_LIB_SUFFIX}" PUBLIC maru_iface${MARU_LIB_SUFFIX}_indirect)

    list(APPEND MARU_INSTALL_TARGETS 
        "maru_vulkan_wayland${MARU_LIB_SUFFIX}" 
        "maru_vulkan_x11${MARU_LIB_SUFFIX}" 
        "maru_vulkan_linux${MARU_LIB_SUFFIX}"
    )

    set_target_properties("maru_vulkan_wayland${MARU_LIB_SUFFIX}" PROPERTIES EXPORT_NAME vulkan_wayland${MARU_LIB_SUFFIX})
    set_target_properties("maru_vulkan_x11${MARU_LIB_SUFFIX}" PROPERTIES EXPORT_NAME vulkan_x11${MARU_LIB_SUFFIX})
    set_target_properties("maru_vulkan_linux${MARU_LIB_SUFFIX}" PROPERTIES EXPORT_NAME vulkan${MARU_LIB_SUFFIX})
endif()

set(MARU_INSTALL_TARGETS ${MARU_INSTALL_TARGETS} PARENT_SCOPE)
