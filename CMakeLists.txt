cmake_minimum_required(VERSION 3.20)
project(Maru VERSION 0.1.0)

# AKA Are we building lvkw as a subdirectory or not?
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    set(MARU_IS_ROOT_CMAKE_PROJECT ON)
else()
    set(MARU_IS_ROOT_CMAKE_PROJECT OFF)
endif()

# Optional modules you can disable if you so chose.
option(MARU_ENABLE_CONTROLLER "Enable controller support" ON)
option(MARU_ENABLE_VULKAN "Enable Vulkan surface binding" ON)

# Debug and diagnostics options

# Disabling diagnostics will notably prevent any error handling string from making it to the final binary.
option(MARU_ENABLE_DIAGNOSTICS "Enable diagnostics reporting through callbacks" ON)

# Metrics allows monitoring watermarks and performance metrics.
option(MARU_GATHER_METRICS "Enable metrics gathering" ON)

# API call validation specifically checks for programmer errors in API usage.
# Typically, you want this enabled during development, and disabled in production.
# The checking is quite thorough, It verifies thread affinity, context and window state, etc...
option(MARU_VALIDATE_API_CALLS "Should API calls be validated?" OFF)

# By default, invalid API calls will cause the library to intentionally abort.
# You CAN make API calls early-out without crashing, but you should only do so if you
# actively need it for some reason (e.g if the calls are being driven by a scripting environment)
option(MARU_RECOVERABLE_API_CALLS "Should invalid API calls be recoverable?" OFF)

# Mostly useful for debugging the library itself.
option(MARU_ENABLE_INTERNAL_CHECKS "Should the library agressively validate internal state" OFF)

# Enables intentional fault-injection hooks used for backend testing.
option(MARU_ENABLE_FAULT_INJECTION "Enable fault injection testing hooks" OFF)

# Auxiliary targets
option(MARU_BUILD_EXAMPLES "Build example applications" ${MARU_IS_ROOT_CMAKE_PROJECT})
option(MARU_BUILD_TESTBED "Build the Maru showcase/testbed" ${MARU_IS_ROOT_CMAKE_PROJECT})

# Arithmetic precision
option(MARU_USE_FLOAT "Use float instead of double for logical coordinates" OFF)

add_subdirectory(src)

# You most likely want to simply use the maru::maru target,
# but you can also refer to a specific backend if you want.
if (WIN32)
  add_library(maru::maru ALIAS maru_win32)
  add_library(maru::windows ALIAS maru_win32)
elseif (APPLE)
  add_library(maru::maru ALIAS maru_macos)
  add_library(maru::macos ALIAS maru_macos)
else()
  add_library(maru::maru ALIAS maru_linux)
  add_library(maru::linux ALIAS maru_linux)
  add_library(maru::wayland ALIAS maru_wayland)
  add_library(maru::x11 ALIAS maru_x11)
endif()

if(LVKW_BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(LVKW_BUILD_TESTBED)
  add_subdirectory(tools/testbed)
endif()

# Installation
install(DIRECTORY include/lvkw DESTINATION include)
install(FILES "${PROJECT_BINARY_DIR}/include/lvkw/details/lvkw_config.h"
        DESTINATION include/lvkw/details)

if (WIN32)
  install(TARGETS lvkw_win32 DESTINATION lib/lvkw)
elseif (APPLE)
  install(TARGETS lvkw_macos DESTINATION lib/lvkw)
else()
  install(TARGETS lvkw_linux lvkw_wayland lvkw_x11 DESTINATION lib/lvkw)
endif()
