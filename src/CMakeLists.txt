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
# Common settings: C11 standard and aggressive warnings
add_library(maru_common_settings INTERFACE)
target_compile_features(maru_common_settings INTERFACE c_std_11)

if(MSVC)
  target_compile_options(maru_common_settings INTERFACE /W4 /WX /wd4100 /wd4189 /wd5105)
else()
  target_compile_options(maru_common_settings INTERFACE
    -Wall
    -Wextra
    -Werror
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
endif()

# Public interface: only public headers
add_library(maru_iface INTERFACE)
target_include_directories(maru_iface INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(maru_iface INTERFACE maru_common_settings)
if(NOT MARU_BUILD_SHARED)
  target_compile_definitions(maru_iface INTERFACE MARU_STATIC)
endif()

# Internal interface: public headers + private core headers
add_library(maru_core_iface INTERFACE)
target_link_libraries(maru_core_iface INTERFACE maru_iface)
target_include_directories(maru_core_iface INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/src/core>
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
add_library("maru_base_obj" OBJECT core/core.c)
target_link_libraries("maru_base_obj" PRIVATE ${MARU_ACTIVE_CORE_IFACE})

add_library("maru_base_indirect_obj" OBJECT core/core.c)
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

add_subdirectory(ext-vulkan)

set(MARU_INSTALL_TARGETS ${MARU_INSTALL_TARGETS} PARENT_SCOPE)
