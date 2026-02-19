# MacOS double flavor
add_library(maru_macos ${MARU_LIB_TYPE} macos_entry.c $<TARGET_OBJECTS:maru_base_d_obj>)
target_link_libraries(maru_macos PRIVATE maru_core_iface_d)
target_link_libraries(maru_macos PUBLIC maru_iface_d_direct)

# MacOS float flavor
add_library(maru_macos_f ${MARU_LIB_TYPE} macos_entry.c $<TARGET_OBJECTS:maru_base_f_obj>)
target_link_libraries(maru_macos_f PRIVATE maru_core_iface_f)
target_link_libraries(maru_macos_f PUBLIC maru_iface_f_direct)
