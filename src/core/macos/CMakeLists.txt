if (MARU_BUILD_UNITY)
  add_library(maru_macos ${MARU_LIB_TYPE} "${PROJECT_SOURCE_DIR}/src/unity/maru_macos.m")
  target_link_libraries(maru_macos PRIVATE ${MARU_ACTIVE_CORE_IFACE})
else()
  add_library(maru_macos ${MARU_LIB_TYPE}
      macos_entry.m 
      macos_context.m 
      macos_window.m 
      macos_cursor.m
      macos_image.m
      macos_controller.m
      macos_dataexchange.m
      macos_monitor.m
      macos_vulkan.m
      $<TARGET_OBJECTS:maru_base_obj>
  )
  target_link_libraries(maru_macos PRIVATE ${MARU_ACTIVE_CORE_IFACE})
endif()
target_link_libraries(maru_macos PUBLIC maru_iface_direct)
target_link_libraries(maru_macos PRIVATE "-framework Cocoa" "-framework QuartzCore" "-framework GameController" "-framework IOKit" "-framework CoreVideo")
