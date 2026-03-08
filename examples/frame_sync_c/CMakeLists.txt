add_executable(maru_frame_sync_c main.c)
target_link_libraries(maru_frame_sync_c PRIVATE maru::maru maru_shared_renderer Vulkan::Vulkan)
target_include_directories(maru_frame_sync_c PRIVATE ${PROJECT_SOURCE_DIR}/include)
