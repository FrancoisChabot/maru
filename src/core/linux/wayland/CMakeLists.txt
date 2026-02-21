set(MARU_WAYLAND_SOURCES 
    wayland_entry.c
    wayland_context.c
    wayland_input.c
    wayland_monitor.c
    wayland_csd.c
    wayland_window.c
    wayland_misc.c
    wayland_utils.c
    dlib/loader.c
    protocols/maru_protocols.c
)

add_library(maru_wayland_internal_iface INTERFACE)
target_include_directories(maru_wayland_internal_iface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/dlib/vendor>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/protocols/generated>
)
target_link_libraries(maru_wayland_internal_iface INTERFACE maru_linux_internal_iface)

install(TARGETS maru_wayland_internal_iface EXPORT MaruTargets)

set(iface_direct "maru_iface${MARU_LIB_SUFFIX}_direct")

# Direct target objects
add_library("maru_wayland_obj" OBJECT ${MARU_WAYLAND_SOURCES})
target_link_libraries("maru_wayland_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE} maru_wayland_internal_iface)
set_property(TARGET "maru_wayland_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

# Indirect target objects
add_library("maru_wayland_indirect_obj" OBJECT ${MARU_WAYLAND_SOURCES})
target_link_libraries("maru_wayland_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE} maru_wayland_internal_iface ${CMAKE_DL_LIBS})
target_compile_definitions("maru_wayland_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
set_property(TARGET "maru_wayland_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

# Standalone library
add_library("maru_wayland${MARU_LIB_SUFFIX}" ${MARU_LIB_TYPE}
    $<TARGET_OBJECTS:maru_wayland_obj>
    $<TARGET_OBJECTS:maru_base_obj>
)
target_link_libraries("maru_wayland${MARU_LIB_SUFFIX}" PUBLIC ${iface_direct})
