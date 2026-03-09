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
file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/include/maru/c/details")
configure_file(
    "core/maru_config.h.in"
    "${PROJECT_BINARY_DIR}/include/maru/c/details/maru_config.h"
)

# 3. Base interfaces
# Common settings: aggressive warnings
add_library(maru_common_settings INTERFACE)

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
    -Wdouble-promotion
    -Wformat=2
    -Wno-unused-parameter
    -Wno-unused-variable
  )
  if(MARU_WERROR)
    target_compile_options(maru_common_settings INTERFACE -Werror)
  endif()
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

find_program(MARU_PYTHON3_EXECUTABLE NAMES python3 REQUIRED)

set(MARU_CURSOR_ASSET_DIR "${PROJECT_SOURCE_DIR}/assets/cursors/macos")
set(MARU_CURSOR_ASSETS
    "${MARU_CURSOR_ASSET_DIR}/default.png"
    "${MARU_CURSOR_ASSET_DIR}/help.png"
    "${MARU_CURSOR_ASSET_DIR}/hand.png"
    "${MARU_CURSOR_ASSET_DIR}/wait.png"
    "${MARU_CURSOR_ASSET_DIR}/crosshair.png"
    "${MARU_CURSOR_ASSET_DIR}/text.png"
    "${MARU_CURSOR_ASSET_DIR}/move.png"
    "${MARU_CURSOR_ASSET_DIR}/not_allowed.png"
    "${MARU_CURSOR_ASSET_DIR}/ew_resize.png"
    "${MARU_CURSOR_ASSET_DIR}/ns_resize.png"
    "${MARU_CURSOR_ASSET_DIR}/nesw_resize.png"
    "${MARU_CURSOR_ASSET_DIR}/nwse_resize.png"
)
set(MARU_CURSOR_GEN_DIR "${PROJECT_BINARY_DIR}/generated/cursors")
set(MARU_CURSOR_GEN_C "${MARU_CURSOR_GEN_DIR}/maru_cursor_assets.c")
set(MARU_CURSOR_GEN_H "${MARU_CURSOR_GEN_DIR}/maru_cursor_assets.h")

add_custom_command(
    OUTPUT "${MARU_CURSOR_GEN_C}" "${MARU_CURSOR_GEN_H}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${MARU_CURSOR_GEN_DIR}"
    COMMAND "${MARU_PYTHON3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/scripts/png_cursor_to_c.py"
            --input-dir "${MARU_CURSOR_ASSET_DIR}"
            --output-c "${MARU_CURSOR_GEN_C}"
            --output-h "${MARU_CURSOR_GEN_H}"
    DEPENDS
            "${PROJECT_SOURCE_DIR}/tools/scripts/png_cursor_to_c.py"
            ${MARU_CURSOR_ASSETS}
    VERBATIM
)

add_custom_target(maru_cursor_asset_codegen
    DEPENDS "${MARU_CURSOR_GEN_C}" "${MARU_CURSOR_GEN_H}"
)

target_include_directories(maru_core_iface INTERFACE
    $<BUILD_INTERFACE:${MARU_CURSOR_GEN_DIR}>
)

# 4. Precision-specific interfaces
add_library(maru_iface_d INTERFACE)
target_link_libraries(maru_iface_d INTERFACE maru_iface)

add_library(maru_iface_f INTERFACE)
target_link_libraries(maru_iface_f INTERFACE maru_iface)
target_compile_definitions(maru_iface_f INTERFACE MARU_USE_FLOAT)

# Internal precision interfaces
add_library(maru_core_iface_d INTERFACE)
target_link_libraries(maru_core_iface_d INTERFACE maru_core_iface maru_iface_d)

add_library(maru_core_iface_f INTERFACE)
target_link_libraries(maru_core_iface_f INTERFACE maru_core_iface maru_iface_f)

# The active precision interface for this build
set(MARU_ACTIVE_CORE_IFACE "maru_core_iface${MARU_LIB_SUFFIX}")
set(MARU_ACTIVE_IFACE "${MARU_PRECISION_IFACE}")

install(TARGETS
    maru_iface
    maru_iface_d
    maru_iface_f
    maru_common_settings
    maru_core_iface
    maru_core_iface_d
    maru_core_iface_f
    EXPORT MaruTargets)

# 5. Specialized interfaces for direct vs indirect
set(target_direct "maru_iface${MARU_LIB_SUFFIX}_direct")
set(target_indirect "maru_iface${MARU_LIB_SUFFIX}_indirect")

# Public flavor interfaces
add_library(${target_direct} INTERFACE)
target_link_libraries(${target_direct} INTERFACE ${MARU_ACTIVE_IFACE})

add_library(${target_indirect} INTERFACE)
target_link_libraries(${target_indirect} INTERFACE ${MARU_ACTIVE_IFACE})
target_compile_definitions(${target_indirect} INTERFACE MARU_INDIRECT_BACKEND)

install(TARGETS ${target_direct} ${target_indirect} EXPORT MaruTargets)

# Base objects for this build
add_library("maru_base_obj" OBJECT core/core.c core/core_event_queue.c core/maru_queue.c "${MARU_CURSOR_GEN_C}")
target_link_libraries("maru_base_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
add_dependencies("maru_base_obj" maru_cursor_asset_codegen)

add_library("maru_base_indirect_obj" OBJECT core/core.c core/core_event_queue.c core/core_indirect_entry.c core/maru_queue.c "${MARU_CURSOR_GEN_C}")
target_link_libraries("maru_base_indirect_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})
target_compile_definitions("maru_base_indirect_obj" PRIVATE MARU_INDIRECT_BACKEND)
add_dependencies("maru_base_indirect_obj" maru_cursor_asset_codegen)

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
