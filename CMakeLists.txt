cmake_minimum_required(VERSION 3.20)
project(Maru VERSION 0.1.0)

# AKA Are we building maru as a subdirectory or not?
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(MARU_IS_ROOT_CMAKE_PROJECT ON)
else()
    set(MARU_IS_ROOT_CMAKE_PROJECT OFF)
endif()

# Optional modules you can disable if you so chose.
option(MARU_ENABLE_CONTROLLERS "Enable controller support" ON)
option(MARU_ENABLE_VULKAN "Enable Vulkan surface binding" ON)

# Debug and diagnostics options
option(MARU_ENABLE_DIAGNOSTICS "Enable diagnostics reporting through callbacks" ON)
option(MARU_GATHER_METRICS "Enable metrics gathering" ON)
option(MARU_VALIDATE_API_CALLS "Should API calls be validated?" OFF)
option(MARU_RECOVERABLE_API_CALLS "Should invalid API calls be recoverable?" OFF)
option(MARU_ENABLE_INTERNAL_CHECKS "Should the library agressively validate internal state" OFF)
option(MARU_ENABLE_FAULT_INJECTION "Enable fault injection testing hooks" OFF)

# Auxiliary targets
option(MARU_BUILD_EXAMPLES "Build example applications" ${MARU_IS_ROOT_CMAKE_PROJECT})
option(MARU_BUILD_TESTS "Build test suite" ${MARU_IS_ROOT_CMAKE_PROJECT})
option(MARU_BUILD_SHARED "Build shared libraries instead of static ones" OFF)

if(MARU_BUILD_SHARED)
  set(MARU_LIB_TYPE SHARED)
else()
  set(MARU_LIB_TYPE STATIC)
endif()

add_subdirectory(src)

# Define library aliases for both flavors
if (WIN32)
  add_library(maru::maru ALIAS maru_windows)
  add_library(maru::maru_f ALIAS maru_windows_f)
elseif (APPLE)
  add_library(maru::maru ALIAS maru_macos)
  add_library(maru::maru_f ALIAS maru_macos_f)
else()
  add_library(maru::maru ALIAS maru_linux)
  add_library(maru::maru_f ALIAS maru_linux_f)
  
  # Backend aliases (default to double)
  add_library(maru::wayland ALIAS maru_wayland_d)
  add_library(maru::x11 ALIAS maru_x11_d)
  
  # Float backend aliases
  add_library(maru::wayland_f ALIAS maru_wayland_f)
  add_library(maru::x11_f ALIAS maru_x11_f)
endif()

if(MARU_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(MARU_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

# Set export names for consistent cross-platform usage
if (WIN32)
  set_target_properties(maru_windows PROPERTIES EXPORT_NAME maru)
  set_target_properties(maru_windows_f PROPERTIES EXPORT_NAME maru_f)
  set(MARU_INSTALL_TARGETS maru_windows maru_windows_f)
elseif (APPLE)
  set_target_properties(maru_macos PROPERTIES EXPORT_NAME maru)
  set_target_properties(maru_macos_f PROPERTIES EXPORT_NAME maru_f)
  set(MARU_INSTALL_TARGETS maru_macos maru_macos_f)
else()
  set_target_properties(maru_linux PROPERTIES EXPORT_NAME maru)
  set_target_properties(maru_linux_f PROPERTIES EXPORT_NAME maru_f)
  set_target_properties(maru_wayland_d PROPERTIES EXPORT_NAME wayland)
  set_target_properties(maru_wayland_f PROPERTIES EXPORT_NAME wayland_f)
  set_target_properties(maru_x11_d PROPERTIES EXPORT_NAME x11)
  set_target_properties(maru_x11_f PROPERTIES EXPORT_NAME x11_f)
  set(MARU_INSTALL_TARGETS maru_linux maru_linux_f maru_wayland_d maru_wayland_f maru_x11_d maru_x11_f)
endif()

# Installation
install(DIRECTORY include/maru DESTINATION include)
install(FILES "${PROJECT_BINARY_DIR}/include/maru/c/details/maru_config.h"
        DESTINATION include/maru/c/details)

install(TARGETS ${MARU_INSTALL_TARGETS}
        EXPORT MaruTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include)

install(EXPORT MaruTargets
        FILE MaruTargets.cmake
        NAMESPACE maru::
        DESTINATION lib/cmake/Maru)

# Create a basic MaruConfig.cmake
file(WRITE "${PROJECT_BINARY_DIR}/MaruConfig.cmake" "include(\"\${CMAKE_CURRENT_LIST_DIR}/MaruTargets.cmake\")\n")
install(FILES "${PROJECT_BINARY_DIR}/MaruConfig.cmake" DESTINATION lib/cmake/Maru)
