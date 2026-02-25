set(MARU_X11_SOURCES
    x11_entry.c
    x11_vulkan.c
)

set(iface_direct "maru_iface${MARU_LIB_SUFFIX}_direct")

# Direct target objects
add_library("maru_x11_obj" OBJECT ${MARU_X11_SOURCES})
target_link_libraries("maru_x11_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
set_property(TARGET "maru_x11_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

# Indirect target objects
add_library("maru_x11_indirect_obj" OBJECT ${MARU_X11_SOURCES})
target_link_libraries("maru_x11_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
target_compile_definitions("maru_x11_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
set_property(TARGET "maru_x11_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

# Standalone library
add_library("maru_x11${MARU_LIB_SUFFIX}" ${MARU_LIB_TYPE}
    $<TARGET_OBJECTS:maru_x11_obj>
    $<TARGET_OBJECTS:maru_base_obj>
)
target_link_libraries("maru_x11${MARU_LIB_SUFFIX}" PUBLIC ${iface_direct})
