# Integrating Maru

Maru is designed to be highly portable and supports several integration paths, ranging from industry-standard CMake to "Bring-Your-Own-Build-System" (BYOBS) via single-file unity builds.

## 1. CMake Integration (Recommended)

If your project already uses CMake, Maru provides first-class support for `FetchContent`, `add_subdirectory`, and installed packages.

### Using FetchContent
This is the simplest way to get up and running quickly.

```cmake
include(FetchContent)
FetchContent_Declare(maru
  GIT_REPOSITORY https://github.com/birdsafe/maru.git
  GIT_TAG <version_tag_or_hash>
)
FetchContent_MakeAvailable(maru)

target_link_libraries(your_target PRIVATE maru::maru)
```

### Using add_subdirectory
If you have Maru as a git submodule or a local folder:

```cmake
add_subdirectory(path/to/maru)
target_link_libraries(your_target PRIVATE maru::maru)
```

### Pre-compiled/Installed Maru
If you have installed Maru to your system or a specific prefix:

```cmake
find_package(Maru REQUIRED)
target_link_libraries(your_target PRIVATE maru::maru)
```

---

## 2. Unity Build (BYOBS)

You can also integrate the MARU source directly in your build systems.

### The Masterfile Approach
A "masterfile" is a single `.c` file that `#include`s the necessary source files for your target platform. When integrating Maru in your build system, you only have to compile the one .c file that matches your target.

In all cases, you need to add Maru's `include/` and `src/` directories to your header search paths.

#### Windows (Win32)
1.  Add `src/unity/maru_windows.c` to your project.
2.  Link against system libraries: `user32.lib`, `gdi32.lib`, `shell32.lib`, `ole32.lib`, `uuid.lib`.

#### macOS (Cocoa)
1.  Add `src/unity/maru_macos.m` (Objective-C) to your project.
2.  Link against Apple frameworks: `Cocoa`, `CoreGraphics`, `QuartzCore`.

#### Linux (Dynamic/Indirect)
This version supports both X11 and Wayland, detecting the environment at runtime.
1.  Add `src/unity/maru_linux_dynamic.c` to your project.
2.  Link against: `libdl`, `libpthread`.

#### Linux (Direct Backend)
If you want to strip out the dynamic selection and target only one display server:
1.  Add `src/unity/maru_wayland.c` or `src/unity/maru_x11.c` to your project.
2.  Link against: `libdl`, `libpthread`.

---

## 3. Configuration Header

Maru relies on a `maru_config.h` file to control optional features like diagnostics and API validation.

When using CMake, this file is generated automatically in your build directory. For BYOBS, you have two options:

1.  **Manual Header:** Copy `include/maru/details/maru_config.h.example` to `include/maru/details/maru_config.h` and edit the values manually.
2.  **Compiler Defines:** Maru's headers are designed to fall back to sane defaults if specific macros are not defined. You can provide these via your build system (e.g., `-DMARU_ENABLE_DIAGNOSTICS=1`).

| Macro | Default | Description |
| :--- | :--- | :--- |
| `MARU_ENABLE_DIAGNOSTICS` | `ON` | Enable error/info reporting via callbacks. |
| `MARU_VALIDATE_API_CALLS` | `ON` | Enable aggressive runtime API usage checks. |
| `MARU_ENABLE_INTERNAL_CHECKS`| `OFF` | Enable heavy internal consistency checks. |
