#ifndef MARU_CONFIG_H_INCLUDED
#define MARU_CONFIG_H_INCLUDED


// Library version
#define MARU_VERSION_MAJOR 0
#define MARU_VERSION_MINOR 1
#define MARU_VERSION_PATCH 0

// WARNING! If you use MARU_USE_FLOAT, make certain that you are linking against maru_f. 
// #define MARU_USE_FLOAT

// Extensions
#define MARU_ENABLE_CONTROLLERS
#define MARU_ENABLE_VULKAN

// Backends
// N.B. Multiple backends is permissible

#define MARU_ENABLE_BACKEND_WAYLAND
#define MARU_ENABLE_BACKEND_X11
/* #define MARU_ENABLE_BACKEND_COCOA*/
/* #define MARU_ENABLE_BACKEND_WINDOWS */

#endif  // MARU_CONFIG_H_INCLUDED
