# macOS Backend (Cocoa)

The macOS backend for Maru is implemented using the Cocoa framework (AppKit). Due to the architectural constraints of AppKit and the `NSApplication` singleton, there are specific limitations regarding context management and threading.

## Threading Constraints

### Main Thread Affinity
All `MARU_Context` operations on macOS **must** occur on the process's main thread. This includes:
- Creating and destroying a context.
- Creating and destroying windows.
- Pumping events via `maru_pumpEvents`.

Attempting to perform these operations on any other thread will result in a failure (if `MARU_VALIDATE_API_CALLS` is enabled, the application will abort).

### Multiple Concurrent Contexts
While it is technically possible to have multiple `MARU_Context` instances active on the main thread, it is generally **not recommended** for the following reasons:

1.  **Shared `NSApplication`:** All contexts share the same `NSApp` singleton. Global application states, such as the application icon or activation policy, are shared and may be overwritten by the most recently updated context.
2.  **Event Stealing:** Pumping events on one context via `maru_pumpEvents` will drain the global macOS event queue. Events meant for windows owned by a different context will be automatically redirected to that context's internal queue. You must ensure that **all** active contexts are pumped regularly to process these redirected events.

### Lifecycle Management
Tearing down a `MARU_Context` and creating a new one on the main thread is fully supported. This is particularly important for test suites or applications that need to restart their windowing subsystem without restarting the entire process.

## Vulkan Integration
Vulkan rendering on macOS is supported via `VK_EXT_metal_surface` and MoltenVK. Maru automatically creates a `CAMetalLayer` for each window to serve as the rendering surface.

Note that while the `MARU_Context` must be managed on the main thread, the resulting `VkSurfaceKHR` can be used by Vulkan rendering commands on any thread, as per the Vulkan specification.
