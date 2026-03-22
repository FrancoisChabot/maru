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
    -Wno-shadow
  )
  if(MARU_WERROR)
    target_compile_options(maru_common_settings INTERFACE -Werror)
  endif()
endif()

# Public interface: only public headers
add_library(maru_iface INTERFACE)
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
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/linux>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/linux/dlib>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/linux/dlib/vendor>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/linux/wayland>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/linux/wayland/dlib/vendor>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/linux/wayland/protocols/generated>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/linux/x11>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/windows>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core/macos>
)

# 4. Specialized interfaces for direct vs indirect
set(target_direct "maru_iface_direct")
set(target_indirect "maru_iface_indirect")

# Public flavor interfaces
add_library(${target_direct} INTERFACE)
target_link_libraries(${target_direct} INTERFACE maru_iface)

add_library(${target_indirect} INTERFACE)
target_link_libraries(${target_indirect} INTERFACE maru_iface)
target_compile_definitions(${target_indirect} INTERFACE MARU_INDIRECT_BACKEND)

install(TARGETS
    maru_iface
    ${target_direct}
    ${target_indirect}
    EXPORT MaruTargets
)

# 5. Core Library Implementation
if (WIN32)
  add_library(maru_windows ${MARU_LIB_TYPE} "unity/maru.c")
  target_link_libraries(maru_windows PUBLIC ${target_indirect} PRIVATE $<BUILD_INTERFACE:maru_core_iface>)
elseif (APPLE)
  add_library(maru_macos ${MARU_LIB_TYPE} "unity/maru.c")
  set_source_files_properties("unity/maru.c" PROPERTIES LANGUAGE OBJC)
  target_link_libraries(maru_macos PUBLIC ${target_indirect} PRIVATE $<BUILD_INTERFACE:maru_core_iface>)
else()
  # Combined Linux library (Indirect)
  add_library(maru_linux ${MARU_LIB_TYPE} "unity/maru.c")
  target_link_libraries(maru_linux PUBLIC ${target_indirect} PRIVATE $<BUILD_INTERFACE:maru_core_iface> Threads::Threads ${CMAKE_DL_LIBS})

  # Separate backends
  add_library(maru_wayland ${MARU_LIB_TYPE} "unity/maru.c")
  target_compile_definitions(maru_wayland PRIVATE MARU_ENABLE_BACKEND_WAYLAND)
  target_link_libraries(maru_wayland PUBLIC ${target_direct} PRIVATE $<BUILD_INTERFACE:maru_core_iface> Threads::Threads ${CMAKE_DL_LIBS})

  add_library(maru_x11 ${MARU_LIB_TYPE} "unity/maru.c")
  target_compile_definitions(maru_x11 PRIVATE MARU_ENABLE_BACKEND_X11)
  target_link_libraries(maru_x11 PUBLIC ${target_direct} PRIVATE $<BUILD_INTERFACE:maru_core_iface> Threads::Threads ${CMAKE_DL_LIBS})
endif()

if(MARU_BUILD_SHARED)
  if (WIN32)
    target_compile_definitions(maru_windows PRIVATE MARU_BUILDING_DLL)
  elseif (APPLE)
    target_compile_definitions(maru_macos PRIVATE MARU_BUILDING_DLL)
  else()
    target_compile_definitions(maru_linux PRIVATE MARU_BUILDING_DLL)
    target_compile_definitions(maru_wayland PRIVATE MARU_BUILDING_DLL)
    target_compile_definitions(maru_x11 PRIVATE MARU_BUILDING_DLL)
  endif()
endif()
