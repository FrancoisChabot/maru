if (MARU_BUILD_UNITY)
  add_library(maru_windows ${MARU_LIB_TYPE} "${PROJECT_SOURCE_DIR}/src/unity/maru_windows.c")
  target_link_libraries(maru_windows PRIVATE ${MARU_ACTIVE_CORE_IFACE} ole32 uuid hid)
else()
  add_library(maru_windows ${MARU_LIB_TYPE} 
      windows_entry.c 
      windows_contexts.c 
      windows_windows.c 
      windows_images.c
      windows_events.c
      windows_controllers.c
      windows_data_exchange.c
      windows_cursors.c
      windows_monitors.c
      windows_vulkan.c
      $<TARGET_OBJECTS:maru_base_obj> 
  )
  target_link_libraries(maru_windows PRIVATE ${MARU_ACTIVE_CORE_IFACE} ole32 uuid hid)
endif()
target_link_libraries(maru_windows PUBLIC maru_iface_direct)
if(MARU_BUILD_SHARED)
  target_compile_definitions(maru_windows PRIVATE MARU_BUILDING_DLL)
endif()
