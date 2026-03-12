# Maru

[![License](https://img.shields.io/badge/license-zlib-blue.svg)](LICENSE.txt)

A brutally strict, high-performance desktop windowing and input layer.

Maru is less flexible than GLFW or SDL by design. It is highly opinionated, has a very strict (but clear!) threading model, and it forces you to do things "right" from the get-go. It asks a bit more of you on day
one, but in exchange you get a much stronger foundation to build on.

- It goes out of its way to provide steady and predictable timing, even during unusual events like controller hot-plugs.
- Ruthless API validation that can be completely stripped out. It will abort() on any invalid API usage in validation builds.
- No dynamic global state unless the OS mandates it.
- It stays in its lane. It provides windowing, surface creation, and user inputs (including DnD, clipboard, and cursors). That's it.

Why is it called Maru? This library was built to support a bird-themed game, so it's named after my pet parrot: Marula.

## Library status

Ready for early adopters. There hasn't been enough testing on enough OS/Machine configurations to blindly trust this yet.

## Quickstart

Run the following, and you should be presented with an ImGui-based playground to mess with the various things Maru provides.

You'll need: 
  - A basic development environment (MSVC / XCode / gcc+make)
  - [git](https://git-scm.com/)
  - [CMake](https://cmake.org/) (3.20 or above)
  - The [Vulkan SDK](https://vulkan.lunarg.com/sdk/home). (N.B. only needed for the examples, the Maru library itself doesn't require it)
  - On Windows and MacOS, we only depend on libraries that come packaged with the dev environment. On Linux, we vendor all needed headers.

```bash 
# Linux/macOS: you may have to do this depending on your environment.
# source path/to/vulkan/sdk/setup-env.sh

git clone https://github.com/birdsafe/maru
cd maru
cmake -S . -B build
cmake --build build --config Release
# On macOS/Linux
./build/examples/maru_testbed
# On Windows
# look for build/examples/Release/maru_testbed.exe, but it may be elsewhere depending on your generator.
```

## Integration

The easiest way to integrate Maru is via CMake (`FetchContent` or `add_subdirectory`).

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

You can also build Maru by itself and use it as a regular library:

```bash
cd path/to/maru
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=path/to/where/to/install/maru ..
cmake --install .
```

## Usage example

- This example is pseudocode because properly initializing a vulkan renderer would drown what is being shown.
- We are also intentionally skipping error handling to focus on the Maru semantics. 
- Check out the [basic_c example](examples/basic_c/main.c) for a more complete example.

```c
// N.B. Maru does not include vulkan headers for you, it does not care if you 
// include them before or after it, and it doesn't require any #define beforehand.
#include <vulkan/vulkan.h>
#include "maru/maru.h"

typedef struct AppState {
  bool keep_running;
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
  MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
  maru_createContext(&create_info, &context);

  // Initialize Vulkan.
  MARU_VkExtensionList vk_extensions = {0};
  maru_getVkExtensions(context, &vk_extensions);
  init_vk_renderer(&app.renderer, vk_extensions.names, vk_extensions.count);

  // create a window
  MARU_Window *window = NULL;
  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru Basic C Example";
  window_info.attributes.dip_size = (MARU_Vec2Dip){800, 600};
  maru_createWindow(context, &window_info, &window);

  // Wait for the window to be ready
  while(!app.renderer.has_surface && app.keep_running) {
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

* **MARU_GATHER_METRICS**: `ON` by default. Setting it to `OFF` will stop the gathering of internal metrics.

* **MARU_ENABLE_INTERNAL_CHECKS**: `OFF` by default. Setting it to `ON` will enable internal validation logic double-checks. Only use this if you suspect a bug in Maru itself.

### Recommended configurations

| Build Type         | `MARU_VALIDATE_API_CALLS` | `MARU_ENABLE_DIAGNOSTICS` | `MARU_GATHER_METRICS` | `MARU_ENABLE_INTERNAL_CHECKS` |
| :---               | :---:                     | :---:                     | :---:                 | :---:                         |
| **Development**    | `ON`                      | `ON`                      | `ON`                  | `OFF`                         |
| **Release**        | `OFF`                     | `ON`                      | `ON`                  | `OFF`                         |
| **Production**     | `OFF`                     | `OFF`                     | `OFF`                 | `OFF`                         |
| **Debugging Maru** | `ON`                      | `ON`                      | `ON`                  | `ON`                          |

**N.B.** `MARU_ENABLE_INTERNAL_CHECKS` can have subtle implications beyond performance in some cases. See [Internal Checks Caveats](docs/dev/internal_checks_caveats.md) for details.


## Why is Maru built the way it is?

### 1. Core Philosophy

#### Deterministic Event Dispatch
**"Why do I need to pass callback and mask to maru_pumpEvents() every time?"**

One of the things that's always bugged me about GLFW is that I am never fully confident *when* my callback is getting invoked. The only way I can be truly 100% sure my callback won't ever be called when I don't want it is simply to not let the library have it at all.

Maru performs direct, synchronous event dispatch. It pulls events from the OS and fires them inline, strictly during the pump. That is the only time your callback comes into play. Forcing you to provide the callback and mask to `maru_pumpEvents()` every pump enshrines this on both sides of the fence.

#### Principled Asynchronicity
**"Is there really no synchronous window creation mechanism?"**

Unfortunately, asynchronous window creation is absolutely necessary to get a smooth experience in certain backends. On top of that, synchronous window creation is too sticky of an API. It's too easy to paint oneself into a corner and run into issues later down the road.

In any case, for a single-window app or game, you can easily work around this with a dedicated pump loop immediately after the creation call, as shown in the usage example.

#### Honest Concurrency: The "Lightweight" Lie
**"Why does Maru create a worker thread in some backends?"**

Maru isn't "lightweight" for the sake of a low binary size; it is lightweight so that your application can run as fast and smoothly as possible. 

Certain OS operations (e.g., scanning for gamepads on Linux via udev or handling device-change notifications on Windows) can block for several milliseconds. If we ran these on your main thread, you would see a hitch in your frame rate every time a controller was plugged in. We offload these non-deterministic blocking calls to a dedicated worker thread to ensure `maru_pumpEvents()` remains fast and predictable, even during unusual OS events.

  #### Control Flow vs. Diagnostics
**"Why are there so few error codes?"**

`MARU_Status` is not meant to tell you *what* went wrong, but rather *what you can do about it*.

- `MARU_SUCCESS`: Everything is fine
- `MARU_FAILURE`: The operation failed, but the context is still sane
- `MARU_ERROR_CONTEXT_LOST`: Something critical went wrong in the context, and that context must be destroyed and rebuilt.

Explanations as to *why* something failed are delivered via the `MARU_DiagnosticCallback`.

Note that making an API call on a lost context is **NOT** an API violation. Instead, it will early-out and return `MARU_ERROR_CONTEXT_LOST`. You don't have to check for it at every turn. Usually, handling it from `maru_pumpEvents()` 
and relying on early-outs for the rest is fine.

---

### 2. Memory & Threading

#### Bring Your Own Allocator

Maru allows you to provide a custom allocator during context creation. The allocator interface requires three function pointers (`alloc_cb`, `realloc_cb`, `free_cb`) and an optional userdata pointer. If you omit them, Maru will default to standard `malloc`/`free`.

```c
MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
create_info.allocator.alloc_cb = my_custom_alloc;
create_info.allocator.realloc_cb = my_custom_realloc;
create_info.allocator.free_cb = my_custom_free;
create_info.allocator.userdata = &my_memory_arena;

maru_createContext(&create_info, &context);
```

#### One Context, One Universe
**"Is Maru Thread-safe?"**

*   Each `MARU_Context` is its own little universe. They are **fully** independent from one another.
*   The thread that creates a `MARU_Context` is its owner thread. 
*   For true cross-platform parity, this should be your main thread. As much as we'd like to provide isolated concurent contexts, it's just not possible on every backend.
*   Functions that return `MARU_Status` must be called from the owner thread.
*   Functions that do not return `MARU_Status` (like `maru_postEvent`, `maru_wakeContext`, and reference counting) are internally synchronized and can be called from any thread.

#### Volatile Hardware, Stable Threads
**"Why do Controllers and Monitors use a retain/release system?"**

Because hardware is physically volatile, and your threads are not. In a multi-threaded engine, your simulation thread might be reading from a `MARU_Controller*` at the exact millisecond the user unplugs it. If Maru automatically freed that memory, you'd segfault.

Instead, you `retain()` a reference. If the device is disconnected, Maru flags it as "lost," but the pointer remains valid in memory until you explicitly `release()` it. This guarantees you will never suffer a use-after-free crash due to unpredictable hardware events.

#### Transient Event Data
**"Can I store event pointers for later?"**

**No.** The `MARU_Event*` pointer, and any data it points to, is only valid for the duration of the callback. This allows Maru to use high-performance internal buffers for event data without performing hundreds of tiny allocations per frame.

---

### 3. Input & High DPI

#### Eat Your Veggies: Proper Text Input
**"How am I supposed to use MARU_TextEditCommittedEvent correctly?"**

Maru only provides a full IME-capable text input mechanism. It forces you to "eat your veggies" because quick-and-dirty keyboard-to-text hacks (like using `MARU_KeyboardEvent`) fail on internationalization, keyboard layouts, and OS-level repeats.

Use the `maru_applyTextEditCommitUtf8()` convenience function to apply commits to a buffer. If you also update `MARU_WINDOW_ATTR_TEXT_INPUT_RECT`, you'll get proper Japanese/Korean/etc. support out-of-the-box.

#### High DPI: Dip vs. Px Space
**"What's the difference between dip vectors and px vectors?"**

All coordinate variables in Maru explicitly include `dip` or `px` in their name. In short: use **dip** units for UI layout and hit testing (to match OS scaling) and **px** units for your rendering viewport and swapchain.

#### Analog Inputs: Polling vs. Events
**"Why is there a mouse motion event but no controller analog event?"**

Unlike a mouse that rests completely still, analog joysticks constantly jitter at a microscopic level due to electrical noise. Emitting an event every time a joystick's value changes by 0.001 would flood the event queue and burn CPU cycles. Therefore, Maru provides events for discrete digital buttons, but forces you to poll continuous analog states (`maru_getControllerAnalogStates`) when you actually need them (typically once per frame).

#### Zero-Latency Mouse Input & Coalescence
**"How does Maru handle high-frequency mouse input?"**

By default, every single mouse movement is dispatched to ensure zero-latency access to raw OS input. If your app cannot handle that volume, Maru provides a `MARU_Queue` utility that supports opt-in **event coalescence**. It combines high-frequency events (like motion or scrolling) into a single event with accumulated deltas while strictly preserving temporal ordering with other events.

---

### 4. Integration & Practicalities

#### Strict Separation of Concerns
**"Does Maru initialize Vulkan for me?"**

No. There are too many architectural decisions involved in renderer initialization. Maru sticks to windows and I/O. It provides exactly two Vulkan helpers:
1. `maru_getVkExtensions()`: To tell you what you need for `vkCreateInstance`.
2. `maru_createVkSurface()`: To create a `VkSurfaceKHR` for a given window.

#### Modern Rendering Only
**"Can I use Maru for OpenGL, DX11, etc.?"**

Not natively. Supporting legacy APIs would break several of the hard timing and state guarantees we provide. However, you can always reclaim the OS-specific window handles and bind them yourself if you are feeling adventurous.

#### Transparent Linux Backends
**"Does Maru use X11 or Wayland?"**

The default mode will try Wayland first and fall back to X11. Every system dependency is manually loaded and checked at runtime, so your app will work as long as either is available. You can also force a specific backend at runtime or link against `maru::x11` or `maru::wayland` to strip the selection machinery entirely.

#### Consistent Attribute Updates
**"What's up with the attribute substructs?"**

Attributes represent properties that can change on the fly. We use the same struct type for both `CreateInfo` (initial state) and `maru_update*` calls (live changes). That keeps the API nice and consistent.

#### Predictable Builds
**"Do I really need CMake to build the library?"**

Yes. The X11 and Wayland backends each need to be compiled twice (direct use vs. runtime-dispatched), which requires specific build logic. We only ensure stability for the CMake path. For other build systems,  headers + precompiled library is the way to go.

## Acknowledgements

While there has been a lot of divergence since, a lot of the design and code was originally inspired by, if not at times lifted from, the outstanding [GLFW](https://glfw.org). At the time of writing, credit goes to Marcus Geelnard (2002-2006) and Camilla Löwy (2006-2019)

The examples' code is derived from [Vulkan Tutorial](https://github.com/Overv/VulkanTutorial), but with modifications.

## License

Maru is licensed under the Zlib license. See [LICENSE.txt](LICENSE.txt) for details.
