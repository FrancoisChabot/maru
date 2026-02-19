set(MARU_WAYLAND_SOURCES wayland_entry.c)

macro(define_wayland_flavor_targets suffix)
    set(iface_direct "maru_iface${suffix}_direct")
    set(core_iface "maru_core_iface${suffix}")
    set(base_obj "maru_base${suffix}_obj")

    # Direct target objects
    add_library("maru_wayland${suffix}_obj" OBJECT ${MARU_WAYLAND_SOURCES})
    target_link_libraries("maru_wayland${suffix}_obj" PRIVATE ${core_iface})
    set_property(TARGET "maru_wayland${suffix}_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    # Indirect target objects
    add_library("maru_wayland${suffix}_indirect_obj" OBJECT ${MARU_WAYLAND_SOURCES})
    target_link_libraries("maru_wayland${suffix}_indirect_obj" PRIVATE ${core_iface})
    target_compile_definitions("maru_wayland${suffix}_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
    set_property(TARGET "maru_wayland${suffix}_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    # Standalone library
    add_library("maru_wayland${suffix}" ${MARU_LIB_TYPE}
        $<TARGET_OBJECTS:maru_wayland${suffix}_obj>
        $<TARGET_OBJECTS:${base_obj}>
    )
    target_link_libraries("maru_wayland${suffix}" PUBLIC ${iface_direct})
endmacro()

define_wayland_flavor_targets("_d")
define_wayland_flavor_targets("_f")

# Standard name aliases for the double version
add_library(maru_wayland ALIAS maru_wayland_d)
