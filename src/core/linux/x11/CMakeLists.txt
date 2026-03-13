set(MARU_X11_SOURCES
    x11_entry.c
    x11_context.c
    x11_window.c
    x11_input.c
    x11_dataexchange.c
    x11_monitor.c
    x11_cursor.c
    x11_vulkan.c
    dlib/loader.c
)

set(iface_direct "maru_iface_direct")

# Direct target objects
add_library("maru_x11_obj" OBJECT ${MARU_X11_SOURCES})
target_include_directories("maru_x11_obj" PRIVATE .. dlib)
target_link_libraries("maru_x11_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
set_property(TARGET "maru_x11_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

# Indirect target objects
add_library("maru_x11_indirect_obj" OBJECT ${MARU_X11_SOURCES})
target_include_directories("maru_x11_indirect_obj" PRIVATE .. dlib)
target_link_libraries("maru_x11_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
target_compile_definitions("maru_x11_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
set_property(TARGET "maru_x11_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

# Standalone library
if (MARU_BUILD_UNITY)
  add_library("maru_x11" ${MARU_LIB_TYPE} "${PROJECT_SOURCE_DIR}/src/unity/maru_x11.c")
  target_link_libraries("maru_x11" PRIVATE ${MARU_ACTIVE_CORE_IFACE} maru_linux_internal_iface)
  target_include_directories("maru_x11" PRIVATE .. dlib)
else()
  add_library("maru_x11" ${MARU_LIB_TYPE}
      $<TARGET_OBJECTS:maru_x11_obj>
      $<TARGET_OBJECTS:maru_base_obj>
  )
endif()
target_link_libraries("maru_x11" PUBLIC ${iface_direct})
