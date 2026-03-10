# Windows & Contexts

Windows and Contexts are the primary objects you will interact with in Maru.

## MARU_Context

A `MARU_Context` represents the library's connection to the operating system's display server (e.g., X11, Wayland, or Win32). It manages global state, monitor information, and the event queue.

### Creation and Destruction

You create a context using `maru_createContext`. It's common to use the `MARU_CONTEXT_CREATE_INFO_DEFAULT` macro to initialize the creation parameters.

```c
MARU_ContextCreateInfo context_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
// Optional: set a diagnostic callback for errors/warnings
context_info.attributes.diagnostic_cb = my_diagnostic_callback;

MARU_Context *context = NULL;
if (maru_createContext(&context_info, &context) != MARU_SUCCESS) {
    // Handle error
}

// ... use the context ...

maru_destroyContext(context);
```

### Context Attributes

- **Diagnostic Callback**: A function that Maru calls to report information, warnings, and errors. Highly recommended for debugging.
- **Tuning**: Backend-specific configuration options (e.g., Wayland decoration modes).

---

## MARU_Window

A `MARU_Window` represents a single OS-level window. Windows are owned by a context and are destroyed when the context is destroyed, though it is recommended to destroy them explicitly.

### Creation

Use `maru_createWindow` to create a window. Like contexts, windows use a "Create Info" struct.

```c
MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
window_info.attributes.title = "My App";
window_info.attributes.logical_size = (MARU_Vec2Dip){1280, 720};
window_info.decorated = true;

MARU_Window *window = NULL;
maru_createWindow(context, &window_info, &window);
```

### Window State & Attributes

You can query the current state of a window using various "is" functions:

- `maru_isWindowVisible(window)`
- `maru_isWindowFocused(window)`
- `maru_isWindowMaximized(window)`
- `maru_isWindowFullscreen(window)`
- `maru_isWindowReady(window)`: Returns true once the OS has fully initialized the window and it's ready for rendering.

### Updating Windows

To change window properties after creation (like title, size, or fullscreen state), use `maru_updateWindow`. You must provide a bitmask indicating which attributes you are updating.

```c
MARU_WindowAttributes attrs = {0};
attrs.title = "New Title";
attrs.fullscreen = true;

// Update both title and fullscreen state
maru_updateWindow(window, MARU_WINDOW_ATTR_TITLE | MARU_WINDOW_ATTR_FULLSCREEN, &attrs);
```

### Window Geometry

`maru_getWindowGeometry` provides a snapshot of the window's size and position in both logical units (DIPs) and physical pixels.

```c
MARU_WindowGeometry geometry = maru_getWindowGeometry(window);

printf("Pixel size: %fx%f\n", geometry.pixel_size.x, geometry.pixel_size.y);
printf("Scale factor: %f\n", geometry.scale_factor);
```

### Lifecycle & the "Ready" State

On some platforms (especially Wayland), a window is not immediately ready for use after `maru_createWindow` returns. You should wait for the `MARU_EVENT_WINDOW_READY` event or poll `maru_isWindowReady()` before attempting to create a rendering surface (like a Vulkan swapchain).

Next: [Input Handling](inputs.md)
