# Vulkan Integration

Maru is designed to work seamlessly with the Vulkan graphics API. It handles the platform-specific extensions and surface creation for you.

## Extensions

To create a Vulkan instance that can render to Maru windows, you need to enable certain instance extensions (like `VK_KHR_xlib_surface` or `VK_KHR_wayland_surface`). Maru provides a list of these extensions based on the current context.

```c
uint32_t extension_count = 0;
const char **extensions = maru_getVkExtensions(context, &extension_count);

VkInstanceCreateInfo create_info = {
    .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
    .enabledExtensionCount = extension_count,
    .ppEnabledExtensionNames = extensions,
    // ...
};

vkCreateInstance(&create_info, NULL, &instance);
```

---

## Surface Creation

Once you have a window and a Vulkan instance, you can create a `VkSurfaceKHR`.

```c
VkSurfaceKHR surface;
maru_createVkSurface(window, instance, vkGetInstanceProcAddr, &surface);
```

### Important: Wait for "Ready"

On some platforms (like Wayland), a window must be fully initialized by the compositor before you can create a surface for it.

**You should wait for the `MARU_EVENT_WINDOW_READY` event or check `maru_isWindowReady(window)` before calling `maru_createVkSurface`.**

```c
// Example waiting loop
while (!maru_isWindowReady(window)) {
    maru_pumpEvents(context, 16, MARU_ALL_EVENTS, handle_event, NULL);
}

// Now it's safe to create the surface
maru_createVkSurface(window, instance, vkGetInstanceProcAddr, &surface);
```

---

## Resizing

When a window is resized, you will receive a `MARU_EVENT_WINDOW_RESIZED` event. This is your cue to recreate your Vulkan swapchain.

The event payload contains the new pixel size of the window, which you should use for the swapchain extent.

```c
if (type == MARU_EVENT_WINDOW_RESIZED) {
    uint32_t width = (uint32_t)event->resized.geometry.pixel_size.x;
    uint32_t height = (uint32_t)event->resized.geometry.pixel_size.y;

    // Recreate swapchain with new width and height
    recreate_swapchain(width, height);
}
```

Next: [Metrics & Instrumentation](metrics.md) or [Backend-specific Guides](index.md#backend-specific-guides)
