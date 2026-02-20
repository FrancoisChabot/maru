set(MARU_LINUX_COMMON_SOURCES
    linux_entry.c
    linux_context.c
    dlib/linux_loader.c
)

add_library(maru_linux_internal_iface INTERFACE)
target_include_directories(maru_linux_internal_iface INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/dlib>
)

macro(define_linux_flavor_targets suffix)
    set(core_iface "maru_core_iface${suffix}")

    # Linux shared logic
    add_library("maru_linux${suffix}_obj" OBJECT ${MARU_LINUX_COMMON_SOURCES})
    target_link_libraries("maru_linux${suffix}_obj" PRIVATE ${core_iface} ${CMAKE_DL_LIBS})
    target_include_directories("maru_linux${suffix}_obj" PRIVATE .)
    set_property(TARGET "maru_linux${suffix}_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

    # Linux shared logic (indirect)
    add_library("maru_linux${suffix}_indirect_obj" OBJECT ${MARU_LINUX_COMMON_SOURCES})
    target_link_libraries("maru_linux${suffix}_indirect_obj" PRIVATE ${core_iface} ${CMAKE_DL_LIBS})
    target_include_directories("maru_linux${suffix}_indirect_obj" PRIVATE .)
    target_compile_definitions("maru_linux${suffix}_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
    set_property(TARGET "maru_linux${suffix}_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})
endmacro()

define_linux_flavor_targets("_d")
define_linux_flavor_targets("_f")

add_subdirectory(wayland)
add_subdirectory(x11)

# Combined Linux library (Indirect)
add_library(maru_linux ${MARU_LIB_TYPE}
    $<TARGET_OBJECTS:maru_linux_d_indirect_obj>
    $<TARGET_OBJECTS:maru_base_d_indirect_obj>
    $<TARGET_OBJECTS:maru_wayland_d_indirect_obj>
    $<TARGET_OBJECTS:maru_x11_d_indirect_obj>
    $<$<BOOL:${MARU_ENABLE_VULKAN}>:$<TARGET_OBJECTS:maru_vulkan_linux_d_obj>>
    $<$<BOOL:${MARU_ENABLE_VULKAN}>:$<TARGET_OBJECTS:maru_vulkan_wayland_d_indirect_obj>>
    $<$<BOOL:${MARU_ENABLE_VULKAN}>:$<TARGET_OBJECTS:maru_vulkan_x11_d_indirect_obj>>
)
target_link_libraries(maru_linux PUBLIC maru_iface_d_indirect)
if(MARU_BUILD_SHARED)
  target_compile_definitions(maru_linux PRIVATE MARU_BUILD_DLL)
endif()

add_library(maru_linux_f ${MARU_LIB_TYPE}
    $<TARGET_OBJECTS:maru_linux_f_indirect_obj>
    $<TARGET_OBJECTS:maru_base_f_indirect_obj>
    $<TARGET_OBJECTS:maru_wayland_f_indirect_obj>
    $<TARGET_OBJECTS:maru_x11_f_indirect_obj>
    $<$<BOOL:${MARU_ENABLE_VULKAN}>:$<TARGET_OBJECTS:maru_vulkan_linux_f_obj>>
    $<$<BOOL:${MARU_ENABLE_VULKAN}>:$<TARGET_OBJECTS:maru_vulkan_wayland_f_indirect_obj>>
    $<$<BOOL:${MARU_ENABLE_VULKAN}>:$<TARGET_OBJECTS:maru_vulkan_x11_f_indirect_obj>>
)
target_link_libraries(maru_linux_f PUBLIC maru_iface_f_indirect)
if(MARU_BUILD_SHARED)
  target_compile_definitions(maru_linux_f PRIVATE MARU_BUILD_DLL)
endif()
