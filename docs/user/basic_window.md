# Basic Window Example

This guide walks you through a slightly more complete example that includes a diagnostic callback and basic window attribute handling.

```c
#include <maru/maru.h>
#include <stdio.h>
#include <stdbool.h>

static bool keep_running = true;

// A simple diagnostic callback to log what Maru is doing.
static void handle_diagnostic(const struct MARU_DiagnosticInfo *info, void *userdata) {
    const char* severity = "INFO";
    if (info->diagnostic >= MARU_DIAGNOSTIC_INVALID_ARGUMENT) {
        severity = "ERROR";
    } else if (info->diagnostic > MARU_DIAGNOSTIC_INFO) {
        severity = "WARNING";
    }
    printf("[MARU %s] %s\n", severity, info->message);
}

// Our main event handler
static void handle_event(MARU_EventId type, MARU_Window *window,
                         const MARU_Event *event, void *userdata) {
    if (type == MARU_EVENT_CLOSE_REQUESTED) {
        keep_running = false;
    } else if (type == MARU_EVENT_KEY_STATE_CHANGED) {
        // Toggle fullscreen when 'F' is pressed
        if (event->key.raw_key == MARU_KEY_F && event->key.state == MARU_BUTTON_STATE_PRESSED) {
            bool is_fullscreen = maru_isWindowFullscreen(window);
            
            MARU_WindowAttributes attrs = {0};
            attrs.fullscreen = !is_fullscreen;
            maru_updateWindow(window, MARU_WINDOW_ATTR_FULLSCREEN, &attrs);
        }
    }
}

int main() {
    // Initialize the context with our diagnostic callback
    MARU_ContextCreateInfo context_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
    context_info.attributes.diagnostic_cb = handle_diagnostic;

    MARU_Context *context = NULL;
    if (maru_createContext(&context_info, &context) != MARU_SUCCESS) {
        return 1;
    }

    // Create a window with specific initial attributes
    MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
    window_info.attributes.title = "Basic Maru Window";
    window_info.attributes.logical_size = (MARU_Vec2Dip){1280, 720};
    window_info.attributes.resizable = true;

    MARU_Window *window = NULL;
    if (maru_createWindow(context, &window_info, &window) != MARU_SUCCESS) {
        maru_destroyContext(context);
        return 1;
    }

    printf("Window created. Press 'F' to toggle fullscreen or close the window to exit.\n");

    // Main event loop
    while (keep_running) {
        // We pump events with a 16ms timeout, which is about 60 Hz.
        // If there are no events, maru_pumpEvents will return after 16ms.
        maru_pumpEvents(context, 16, MARU_ALL_EVENTS, handle_event, NULL);
    }

    // Cleanup
    maru_destroyWindow(window);
    maru_destroyContext(context);

    return 0;
}
```

Next: [Event Queues](queue.md)
