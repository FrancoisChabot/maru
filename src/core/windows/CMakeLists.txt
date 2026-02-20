add_library(maru_windows${MARU_LIB_SUFFIX} ${MARU_LIB_TYPE} 
    windows_entry.c 
    windows_context.c 
    windows_window.c 
    $<TARGET_OBJECTS:maru_base_obj> 
    $<TARGET_OBJECTS:maru_vulkan_windows_obj>
)
target_link_libraries(maru_windows${MARU_LIB_SUFFIX} PRIVATE ${MARU_ACTIVE_CORE_IFACE})
target_link_libraries(maru_windows${MARU_LIB_SUFFIX} PUBLIC maru_iface${MARU_LIB_SUFFIX}_direct)
if(MARU_BUILD_SHARED)
  target_compile_definitions(maru_windows${MARU_LIB_SUFFIX} PRIVATE MARU_BUILD_DLL)
endif()
