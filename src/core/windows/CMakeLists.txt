# Windows double flavor
add_library(maru_windows ${MARU_LIB_TYPE} windows_entry.c windows_context.c windows_window.c $<TARGET_OBJECTS:maru_base_d_obj> $<TARGET_OBJECTS:maru_vulkan_windows_d_obj>)
target_link_libraries(maru_windows PRIVATE maru_core_iface_d)
target_link_libraries(maru_windows PUBLIC maru_iface_d_direct)
if(MARU_BUILD_SHARED)
  target_compile_definitions(maru_windows PRIVATE MARU_BUILD_DLL)
endif()

# Windows float flavor
add_library(maru_windows_f ${MARU_LIB_TYPE} windows_entry.c windows_context.c windows_window.c $<TARGET_OBJECTS:maru_base_f_obj> $<TARGET_OBJECTS:maru_vulkan_windows_f_obj>)
target_link_libraries(maru_windows_f PRIVATE maru_core_iface_f)
target_link_libraries(maru_windows_f PUBLIC maru_iface_f_direct)
if(MARU_BUILD_SHARED)
  target_compile_definitions(maru_windows_f PRIVATE MARU_BUILD_DLL)
endif()
