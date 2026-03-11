cmake_minimum_required(VERSION 3.20)
project(Maru VERSION 0.1.0)

# AKA Are we building maru as a subdirectory or not?
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(MARU_IS_ROOT_CMAKE_PROJECT ON)
else()
    set(MARU_IS_ROOT_CMAKE_PROJECT OFF)
endif()

# Debug and diagnostics options
option(MARU_ENABLE_DIAGNOSTICS "Enable diagnostics reporting through callbacks" ON)
option(MARU_GATHER_METRICS "Enable metrics gathering" ON)
option(MARU_VALIDATE_API_CALLS "Should API calls be validated?" ON)
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

# Define library aliases
if (WIN32)
  add_library(maru::maru ALIAS maru_windows)
elseif (APPLE)
  add_library(maru::maru ALIAS maru_macos)
else()
  add_library(maru::maru ALIAS maru_linux)
  
  # Backend aliases
  add_library(maru::wayland ALIAS maru_wayland)
  add_library(maru::x11 ALIAS maru_x11)
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
  list(APPEND MARU_INSTALL_TARGETS maru_windows)
elseif (APPLE)
  set_target_properties(maru_macos PROPERTIES EXPORT_NAME maru)
  list(APPEND MARU_INSTALL_TARGETS maru_macos)
else()
  set_target_properties(maru_linux PROPERTIES EXPORT_NAME maru)
  set_target_properties(maru_wayland PROPERTIES EXPORT_NAME wayland)
  set_target_properties(maru_x11 PROPERTIES EXPORT_NAME x11)
  list(APPEND MARU_INSTALL_TARGETS maru_linux maru_wayland maru_x11)
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
