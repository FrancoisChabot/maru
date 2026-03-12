# Getting Started with Maru

Maru is designed to be easy to integrate into existing CMake projects.

## Installation & Integration

The recommended way to use Maru is via CMake's `FetchContent`.

```cmake
include(FetchContent)
FetchContent_Declare(
    maru
    GIT_REPOSITORY https://github.com/birdsafe/maru.git
    GIT_TAG main # Or a specific version tag
)
FetchContent_MakeAvailable(maru)

# Link against Maru in your executable or library
target_link_libraries(my_app PRIVATE maru::maru)
```

## Basic Concepts

Maru revolves around a few key objects:

1.  **`MARU_Context`**: The main library state. It manages connection to the display server (X11, Wayland, Win32, Cocoa).
2.  **`MARU_Window`**: A single native window.
3.  **Event Pump**: Maru uses a polling-based event system. You call `maru_pumpEvents` to receive and handle events from the OS.

## A Simple Program

Here is the minimal code to create a window and keep it open until the user closes it.

```c
#include <maru/maru.h>
#include <stdio.h>
#include <stdbool.h>

static bool keep_running = true;

static void handle_event(MARU_EventId type, MARU_Window *window,
                         const MARU_Event *event, void *userdata) {
    if (type == MARU_EVENT_CLOSE_REQUESTED) {
        keep_running = false;
    }
}

int main() {
    // 1. Create a context
    MARU_ContextCreateInfo context_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    MARU_Context *context = NULL;
    if (maru_createContext(&context_info, &context) != MARU_SUCCESS) {
        return 1;
    }

    // 2. Create a window
    MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
    window_info.attributes.title = "Hello Maru";
    MARU_Window *window = NULL;
    if (maru_createWindow(context, &window_info, &window) != MARU_SUCCESS) {
        maru_destroyContext(context);
        return 1;
    }

    // 3. Main loop
    while (keep_running) {
        // Pump events for up to 16ms (matches ~60fps)
        maru_pumpEvents(context, 16, MARU_ALL_EVENTS, handle_event, NULL);
    }

    // 4. Cleanup
    maru_destroyWindow(window);
    maru_destroyContext(context);

    return 0;
}
```

Next: [Event Queues](queue.md) or [Windows & Contexts](windows.md)
