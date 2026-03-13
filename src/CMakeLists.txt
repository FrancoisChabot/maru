# 1. Determine which backends are enabled
if (NOT WIN32)
  find_package(Threads REQUIRED)
endif()

if (WIN32)
  set(MARU_ENABLE_BACKEND_WINDOWS ON)
elseif (APPLE)
  set(MARU_ENABLE_BACKEND_COCOA ON)
else()
  set(MARU_ENABLE_BACKEND_WAYLAND ON)
  set(MARU_ENABLE_BACKEND_X11 ON)
endif()

# 2. Configure the public config header
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/include/maru/details")
configure_file(
    "core/maru_config.h.in"
    "${PROJECT_BINARY_DIR}/include/maru/details/maru_config.h"
)

# 3. Base interfaces
# Common settings: aggressive warnings
add_library(maru_common_settings INTERFACE)

if(WIN32)
  target_compile_definitions(maru_common_settings INTERFACE UNICODE _UNICODE)
endif()

option(MARU_WERROR "Treat warnings as errors" ON)

if(MSVC)
  target_compile_options(maru_common_settings INTERFACE 
    /W4 /wd4100 /wd4189 /wd5105 /wd4324 /wd4210 /wd4244 /Zc:__cplusplus
    $<$<COMPILE_LANGUAGE:C>:/std:c11 /experimental:c11atomics>
  )
  target_compile_definitions(maru_common_settings INTERFACE _CRT_SECURE_NO_WARNINGS)
  if(MARU_WERROR)
    target_compile_options(maru_common_settings INTERFACE /WX)
  endif()
else()
  target_compile_options(maru_common_settings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wcast-align
    -Wunused
    -Wconversion
    -Wsign-conversion
    -Wnull-dereference
    -Wformat=2
    -Wno-unused-parameter
    -Wno-unused-variable
  )
  if(MARU_WERROR)
    target_compile_options(maru_common_settings INTERFACE -Werror)
  endif()
endif()

if(MARU_BUILD_UNITY AND NOT MSVC)
  target_compile_options(maru_common_settings INTERFACE -Wno-shadow)
endif()

# Public interface: only public headers
add_library(maru_iface INTERFACE)
target_link_libraries(maru_iface INTERFACE maru_common_settings)
target_compile_features(maru_iface INTERFACE c_std_11)
target_include_directories(maru_iface INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
if(NOT MARU_BUILD_SHARED)
  target_compile_definitions(maru_iface INTERFACE MARU_STATIC)
endif()

# Internal interface: public headers + private core headers + internal warnings
add_library(maru_core_iface INTERFACE)
target_link_libraries(maru_core_iface INTERFACE maru_iface maru_common_settings)
target_include_directories(maru_core_iface INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core>
)

# 4. Final precision-specific interfaces
set(MARU_ACTIVE_CORE_IFACE "maru_core_iface")
set(MARU_ACTIVE_IFACE "maru_iface")

install(TARGETS
    maru_iface
    maru_common_settings
    maru_core_iface
    EXPORT MaruTargets
)

# 5. Specialized interfaces for direct vs indirect
set(target_direct "maru_iface_direct")
set(target_indirect "maru_iface_indirect")

# Public flavor interfaces
add_library(${target_direct} INTERFACE)
target_link_libraries(${target_direct} INTERFACE ${MARU_ACTIVE_IFACE})

add_library(${target_indirect} INTERFACE)
target_link_libraries(${target_indirect} INTERFACE ${MARU_ACTIVE_IFACE})
target_compile_definitions(${target_indirect} INTERFACE MARU_INDIRECT_BACKEND)

install(TARGETS ${target_direct} ${target_indirect} EXPORT MaruTargets)

# Base objects for this build
add_library("maru_base_obj" OBJECT core/core.c core/core_event_queue.c core/maru_queue.c)
target_link_libraries("maru_base_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})

add_library("maru_base_indirect_obj" OBJECT core/core.c core/core_event_queue.c core/core_indirect_entry.c core/maru_queue.c)
target_link_libraries("maru_base_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
target_compile_definitions("maru_base_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)

if (NOT WIN32)
  target_link_libraries("maru_base_obj" PRIVATE Threads::Threads)
  target_link_libraries("maru_base_indirect_obj" PRIVATE Threads::Threads)
endif()

set_property(TARGET "maru_base_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})
set_property(TARGET "maru_base_indirect_obj" PROPERTY POSITION_INDEPENDENT_CODE ${MARU_BUILD_SHARED})

# 6. Add platform-specific subdirectories
if (WIN32)
  add_subdirectory(core/windows)
elseif (APPLE)
  add_subdirectory(core/macos)
else()
  add_subdirectory(core/linux)
endif()

set(MARU_INSTALL_TARGETS ${MARU_INSTALL_TARGETS} PARENT_SCOPE)
