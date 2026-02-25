set(MARU_LINUX_COMMON_SOURCES
    linux_entry.c
    linux_context.c
    linux_controllers.c
    dlib/linux_loader.c
)

add_library(maru_linux_internal_iface INTERFACE)
target_include_directories(maru_linux_internal_iface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/dlib>
)

install(TARGETS maru_linux_internal_iface EXPORT MaruTargets)

# Linux shared logic
add_library("maru_linux_obj" OBJECT ${MARU_LINUX_COMMON_SOURCES})
target_link_libraries("maru_linux_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE} ${CMAKE_DL_LIBS})
target_include_directories("maru_linux_obj" PRIVATE .)
set_property(TARGET "maru_linux_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

# Linux shared logic (indirect)
add_library("maru_linux_indirect_obj" OBJECT ${MARU_LINUX_COMMON_SOURCES})
target_link_libraries("maru_linux_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE} ${CMAKE_DL_LIBS})
target_include_directories("maru_linux_indirect_obj" PRIVATE .)
target_compile_definitions("maru_linux_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
set_property(TARGET "maru_linux_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

add_subdirectory(wayland)
add_subdirectory(x11)

# Combined Linux library (Indirect)
add_library(maru_linux${MARU_LIB_SUFFIX} ${MARU_LIB_TYPE}
    $<TARGET_OBJECTS:maru_linux_indirect_obj>
    $<TARGET_OBJECTS:maru_base_indirect_obj>
    $<TARGET_OBJECTS:maru_wayland_indirect_obj>
    $<TARGET_OBJECTS:maru_x11_indirect_obj>
)
target_link_libraries(maru_linux${MARU_LIB_SUFFIX} PUBLIC maru_iface${MARU_LIB_SUFFIX}_indirect)
if(MARU_BUILD_SHARED)
  target_compile_definitions(maru_linux${MARU_LIB_SUFFIX} PRIVATE MARU_BUILD_DLL)
endif()
