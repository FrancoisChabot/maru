# Maru

[![License](https://img.shields.io/badge/license-zlib-blue.svg)](LICENSE.txt)

Maru is a low-level cross platform windowing library that prioritizes predictability, stability, and performance over convenience. It may ask a bit more of you on day one, but in exchange it gives you a reliable foundation to build on.

- It has no dynamic global state unless the OS mandates it.
- It has a clear and simple, albeit strict, threading model.
- It goes out of its way to provide steady and predictable timing, even during unusual events like controller hot-plugs.
- It has ruthless API validation that can be completely stripped out. It will abort() on any invalid API usage in validation builds.
- It stays in its lane. It provides windowing (including OS features you'd expect like cursors and DnD), surface creation, and user inputs. That's it.

If you are wondering why is it called Maru, this library was built to support a bird-themed game, so it's named after my pet parrot: Marula.

## Library status

Ready for early adopters. There hasn't been enough testing on enough OS/Machine configurations to blindly trust this yet.

### Supported platforms:

No surprises here:

- macOS: 10.15 and up for full support.
- Windows: 10 and up
- Linux: Both X11 and Wayland. 
  - Tested: Ubuntu + GNOME 
  - Tested: Arch + KDE/Plasma

## Quickstart

Run the following, and you should be presented with an ImGui-based playground to mess with the various things Maru provides.

You'll need: 
  - A basic development environment (MSVC / XCode / gcc+make)
  - [CMake](https://cmake.org/) (3.20 or above)
  - The [Vulkan SDK](https://vulkan.lunarg.com/sdk/home). 
    - The full SDK is only necessary to build the examples.
    - Building Maru itself doesn't require anything vulkan-specific.
  - On Windows and MacOS, we only depend on libraries that come packaged with the dev environment. On Linux, we vendor all needed headers.

```bash 
cd maru
cmake -S . -B build
cmake --build build --config Release
# On macOS/Linux
./build/examples/maru_testbed
# On Windows
# look for build/examples/Release/maru_testbed.exe, but it may be elsewhere depending on your generator.
```

## Integration

The easiest way to integrate Maru is via CMake (`add_subdirectory` or `FetchContent`).

```cmake
add_subdirectory(path/to/maru)
target_link_libraries(your_target PRIVATE maru::maru)
```

or

```cmake
include(FetchContent)
FetchContent_Declare(maru
  GIT_REPOSITORY https://github.com/birdsafe/maru.git
  GIT_TAG <target_version>
)
FetchContent_MakeAvailable(maru)
target_link_libraries(your_target PRIVATE maru::maru)
```

For more integrations methods, including injecting Maru's sources in your own build system, check out the [integration guide](docs/user/integration.md).

## Usage example

- This example is not complete because properly initializing a vulkan renderer would drown what is being shown.
- We are also intentionally skipping error handling to focus on the Maru semantics. 
- Check out the [basic_c example](examples/basic_c/main.c) for a more complete example.

```c
#include <vulkan/vulkan.h>
#include "maru/maru.h"
#include "maru/vulkan.h"

typedef struct AppState {
  bool keep_running;
  bool ready;
  VkSurfaceKHR surface;
  MyVkRenderer renderer;
} AppState;

static void handle_event(MARU_EventId type, MARU_Window *window,
                         const MARU_Event *event, void *userdata) {
  AppState* app = (AppState*)userdata;
  
  switch(type) {
    case MARU_EVENT_CLOSE_REQUESTED:
      app->keep_running = false;
      break;
    case MARU_EVENT_WINDOW_READY:
      maru_createVkSurface(window, app->renderer.instance, vkGetInstanceProcAddr, &app->surface);
      renderer_init_surface(&app->renderer, app->surface);
      app->ready = true;
      break;
    default: 
      break;
  }
}

int main() {
  AppState app = {0};
  app.keep_running = true;
  
  // Create a maru context
  MARU_Context *context = NULL;
  MARU_ContextCreateInfo ctx_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  maru_createContext(&ctx_info, &context);

  // Initialize Vulkan.
  MARU_StringList vk_extensions = {0};
  maru_getVkExtensions(context, &vk_extensions);
  init_vk_renderer(&app.renderer, vk_extensions.strings, vk_extensions.count);

  // create a window
  MARU_Window *window = NULL;
  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru Basic C Example";
  window_info.attributes.dip_size = (MARU_Vec2Dip){800, 600};
  maru_createWindow(context, &window_info, &window);

  // Wait for the window to be ready
  while(!app.ready && app.keep_running) {
    // MARU_NEVER blocks until *any* event happens. The app is still alive when waiting for the window. 
    maru_pumpEvents(context, MARU_NEVER /*timeout, in ms*/, MARU_ALL_EVENTS, handle_event, &app);
  }

  // main loop
  while (app.keep_running) {
    maru_pumpEvents(context, 0, MARU_ALL_EVENTS, handle_event, &app);
    renderer_draw_frame(&app.renderer);
  }

  destroy_vk_renderer(&app.renderer);

  // Destroying the context destroys everything attached to it.
  maru_destroyContext(context);

  return 0;
}
```

Consult the `examples/` directory for more.

## Safety, Validation and diagnostics

Maru provides a few different options to control the validation behavior. These are all fixed at the time of compiling the library.

* **MARU_VALIDATE_API_CALLS**: `ON` by default. It drives aggressive validation of the API use. This includes thread affinity checks, state validation, the works. You'll want to set it to `OFF` for releases.

* **MARU_ENABLE_DIAGNOSTICS**: `ON` by default. Setting it to `OFF` will strip virtually all logging and reporting overhead from the library, including the string literals.

* **MARU_ENABLE_INTERNAL_CHECKS**: `OFF` by default. Setting it to `ON` will enable internal validation logic double-checks. Only use this if you suspect a bug in Maru itself.

### Recommended configurations

| Build Type         | `MARU_VALIDATE_API_CALLS` | `MARU_ENABLE_DIAGNOSTICS` | `MARU_ENABLE_INTERNAL_CHECKS` |
| :---               | :---:                     | :---:                     | :---:                         |
| **Development**    | `ON`                      | `ON`                      | `OFF`                         |
| **Release**        | `OFF`                     | `ON`                      | `OFF`                         |
| **Production**     | `OFF`                     | `OFF`                     | `OFF`                         |
| **Debugging Maru** | `ON`                      | `ON`                      | `ON`                          |

## Why is Maru built the way it is?

### Core Principles

#### Asynchronicity

Let's address the elephant in the room up-front: Asynchronous window creation will likely be an annoyance to you if you are coming from GLFW. Windows cannot be used immediately after creation. You have to wait for either a MARU_WINDOW_READY event, or until `maru_isWindowReady()` returns true.

Unfortunately, asynchronous window creation is absolutely necessary to get a smooth experience in certain backends. On top of that, synchronous window creation is too sticky of an API. It's too easy to paint oneself into a corner and run into issues later down the road.

In any case, for a single-window app or game, you can easily work around this with a dedicated pump loop immediately after the creation call, as shown in the usage example.

#### Deterministic Event Dispatch

Maru performs direct, synchronous event dispatch. It pulls events from the OS and fires them inline, strictly during the pump. That is the only time your callback comes into play. Forcing you to provide the callback and mask to `maru_pumpEvents()` every pump enshrines this on both sides of the fence.


#### Control Flow vs. Diagnostics

`MARU_Status` is not meant to tell you *what* went wrong, but rather *what you can do about it*.

- `MARU_SUCCESS`: Everything is fine.
- `MARU_FAILURE`: The operation failed, but the context is still sane.
- `MARU_CONTEXT_LOST`: Something critical went wrong in the context, and that context must be destroyed and rebuilt.

Explanations as to *why* something failed are delivered via the `MARU_DiagnosticCallback`.

Making an API call on a lost context is **NOT** an API violation. Instead, it will early-out and return `MARU_CONTEXT_LOST`. You don't have to check for it at every turn. Usually, handling it from `maru_pumpEvents()` and relying on early-outs for the rest is fine.

### Memory and threading

#### Bring Your Own Allocator

Of course, Maru allows you to provide a custom allocator during context creation. The allocator interface requires three function pointers (`alloc_cb`, `realloc_cb`, `free_cb`) and an optional userdata pointer. If you omit them, Maru will default to standard `malloc`/`free`.

```c
MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
create_info.allocator.alloc_cb = my_custom_alloc;
create_info.allocator.realloc_cb = my_custom_realloc;
create_info.allocator.free_cb = my_custom_free;
create_info.allocator.userdata = &my_memory_arena;

maru_createContext(&create_info, &context);
```

#### Memory lifetimes

Memory handed over to Maru is copied internally unless a specific API explicitly calls for zero-copy retention.

Memory allocated by Maru falls in 4 distinct scopes:

- User-created handles (Contexts, Windows) are valid until destroyed
- Maru-created handles (Controllers, Monitors), are valid until the next event pump unless retained.
- Events and memory pointed to by Events are only valid for the duration of the callback.
- Every thing else is valid until the next pump.

#### Threading model

*   Each `MARU_Context` is its own little universe. They are **fully** independent from one another.
*   The thread that creates a `MARU_Context` is its owner thread. 
*   Functions that return `MARU_Status` (e.g., `maru_createWindow`, `maru_pumpEvents`) MUST be called from the owner thread of the context.
*   `maru_postEvent()`, `maru_wakeContext()`, `maru_*retain()` and `maru_*release()` are globally threading-safe.
*   Other functions can be called from arbitrary threads, provided that they are externally synchronized.

#### Volatile Hardware

Hardware is physically volatile. In a multi-threaded engine, your simulation thread might be reading from a `MARU_Controller*` at the exact millisecond the user unplugs it. If Maru automatically freed that memory, you'd segfault.

In order for Maru to know when it is safe to release the resources' handles, they are managed via refcounting. Application code is expected to use `maru_*retain()` and `maru_*release()` to mark resources that are in active use.

If a retained device is disconnected, Maru flags it as "lost," but the pointer remains valid in memory until you explicitly `release()` it. This guarantees you will never suffer a use-after-free crash due to unpredictable hardware events.

Once a monitor or controller handle is lost, operations that were legal before loss remain legal. Accessors continue to expose the last known snapshot, which may be stale, while backend-facing mutators become inert and return `MARU_FAILURE`.

### Input

#### Text Input

Maru only provides a full IME-capable text input mechanism. It forces you to "eat your veggies" because quick-and-dirty keyboard-to-text hacks (like using `MARU_KeyChangedEvent`) fail on internationalization, keyboard layouts, key repeats, etc...

The examples include a ready-made UTF-8 commit helper in `examples/support/ime_utils.[h|c]`. Copy it into your app if you want a reference implementation for applying `MARU_TextEditCommittedEvent` commits to a buffer.

#### Analog Inputs: Polling vs. Events

Unlike a mouse that rests completely still, analog joysticks constantly jitter. Emitting an event every time a joystick's value changes by 0.001 would flood the event queue and burn CPU cycles. Therefore, Maru provides events for discrete digital buttons, but forces you to poll continuous analog states (`maru_getControllerAnalogStates`) when you actually need them.

#### Mouse Input & Coalescence

By default, every single mouse movement is dispatched to ensure the highest fidelity data is available. If your app cannot handle that volume, Maru provides a standalone `MARU_Queue` utility in `maru/queue.h` that supports opt-in **event coalescence**. It combines high-frequency events (like motion or scrolling) into a single event with accumulated deltas while strictly preserving temporal ordering with other events.

### High DPI

All coordinate variables in Maru explicitly include `dip` or `px` in their name. In short: use **dip** units for UI layout and hit testing (to match OS scaling) and **px** units for your rendering viewport and swapchain.

### Integration & Practicalities

#### Vulkan

Maru provides exactly two Vulkan helpers in `maru/vulkan.h`:

1. `maru_getVkExtensions()`: To tell you what you need for `vkCreateInstance`.
2. `maru_createVkSurface()`: To create a `VkSurfaceKHR` for a given window.

#### Non-vulkan APIs

Supporting OpenGL or older DirectX versions would require breaking some of the hard guarantees Maru is operating under. They are not supported, and are unlikely to be in the future. 

First-class DX12 and Metal are on the roadmap. Let us know if you need them so that we can prioritize accorsdingly.

Maru does provide direct access to the underlying OS window handles as an escape hatch in the meantime.

#### Linux backends: X11 vs Wayland

The default mode will try Wayland first and fall back to X11. Every system dependency is manually loaded and checked at runtime, so your app will work as long as either is available. You can also force a specific backend at runtime or link against `maru::x11` or `maru::wayland` to strip the selection machinery entirely.

### Performance details

#### Cache-friendly Events

Every `MARU_Event` is strictly limited to 64 bytes to ensure it fits within a single standard CPU cache line. This does mean that the chunkier events (e.g. Drag-and-Drop) hold pointers to transient memory, making them more complex to copy around. It's an intentional compromise (performance over convenience, remember?). The optional queue utility is also optimized accordingly out-of-the-box.

## Acknowledgements

While there has been a lot of divergence since, a lot of the design and code was originally inspired by, if not at times lifted from, the outstanding [GLFW](https://glfw.org). At the time of writing, credit goes to Marcus Geelnard (2002-2006) and Camilla Löwy (2006-2019)

The examples' code is derived from [Vulkan Tutorial](https://github.com/Overv/VulkanTutorial), but with modifications.

## License

Maru is licensed under the Zlib license. See [LICENSE.txt](LICENSE.txt) for details.
