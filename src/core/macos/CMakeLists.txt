add_library(maru_macos${MARU_LIB_SUFFIX} ${MARU_LIB_TYPE} 
    macos_entry.c 
    macos_context.c 
    macos_window.c 
    $<TARGET_OBJECTS:maru_base_obj>
)
target_link_libraries(maru_macos${MARU_LIB_SUFFIX} PRIVATE ${MARU_ACTIVE_CORE_IFACE})
target_link_libraries(maru_macos${MARU_LIB_SUFFIX} PUBLIC maru_iface${MARU_LIB_SUFFIX}_direct)
target_link_libraries(maru_macos${MARU_LIB_SUFFIX} PRIVATE "-framework Cocoa")
