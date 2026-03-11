# Maru

[![License](https://img.shields.io/badge/license-zlib-blue.svg)](LICENSE.txt)

A brutally strict, high-performance desktop windowing and input layer.

Maru may be harder to use than GLFW or SDL for a quick prototype, but its goal is to ensure that what you build on day one will take you all the way to the finish line.

What makes Maru neat?
- It goes out of its way to provide steady and predictable timing, even during unusual events like controller hot-plugs.
- Ruthless API validation that can be completely stripped out. It will abort() on any invalid API usage.
- Bring your own allocator, on a per-context basis.
- No dynamic global state whatsoever unless OS-mandated.

Why is it called Maru? This library was built to support a bird-themed game, so it's named after my pet parrot: Marula.

## Library status

Ready for early adopters. There hasn't been enough testing on enough OS/Machine configurations to blindly trust this yet.

## Quickstart

Run the following, and you should be presented with an ImGui-based playground to mess with the various things Maru provides.

You'll need: 
  - A basic development environment (MVSC /XCode / gcc+make)
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
  uint32_t vk_extension_count = 0;
  const char **vk_extensions = maru_getVkExtensions(context, &vk_extension_count);
  init_vk_renderer(&app.renderer, vk_extensions, vk_extension_count);

  // create a window
  MARU_Window *window = NULL;
  MARU_WindowCreateInfo window_info = MARU_WINDOW_CREATE_INFO_DEFAULT;
  window_info.attributes.title = "Maru Basic C Example";
  window_info.attributes.logical_size = (MARU_Vec2Dip){800, 600};
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


## FAQs

### How do I use a custom allocator?

Maru allows you to provide a custom allocator during context creation to guarantee zero hidden allocations. The allocator interface requires three function pointers (`alloc_cb`, `realloc_cb`, `free_cb`) and an optional userdata pointer. If you omit them altogether, Maru will use `malloc()`/`realloc()`/`free()`.

```c
// Example of a custom allocator setup
MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
create_info.allocator.alloc_cb = my_custom_alloc;
create_info.allocator.realloc_cb = my_custom_realloc;
create_info.allocator.free_cb = my_custom_free;
create_info.allocator.userdata = &my_memory_arena;

maru_createContext(&create_info, &context);
```

### How does error handling work?

Maru distinguishes between **Control Flow** and **Diagnostics**. 

`MARU_Status`, used by operations that can fail in normal control flow, is not meant to tell you *what* went wrong, but rather *what you can do about it*. 
- `MARU_SUCCESS`: All good
- `MARU_FAILURE`: That operation failed, but you can still proceed
- `MARU_ERROR_CONTEXT_LOST`: The context is dead in the water (e.g. because the display server went AWOL). You have to tear it down and rebuild from scratch.

For deeper, human-readable errors and warnings—such as failing to load a dynamic library, dropping an invalid OS event, or backend-specific initialization failures—you should attach a `MARU_DiagnosticCallback` to your context.

```c
static void my_diagnostic_cb(const MARU_DiagnosticInfo *info, void *userdata) {
    fprintf(stderr, "Maru Diagnostic [%d]: %s\n", info->diagnostic, info->message);
}

// Attach during context creation or via maru_updateContext
MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
create_info.attributes.diagnostic_cb = my_diagnostic_cb;
maru_createContext(&create_info, &context);
```

### Is Maru Thread-safe?

- Each `MARU_Context` is its own little universe. They are **fully** independent from one-another.
- The thread that creates a `MARU_Context` becomes that context's owner thread.
- If you want true cross-platform parity, `MARU_Context` should live on the main thread. As much as we'd like to support concurrent contexts in different threads, it's just not possible in every single backend.
- Unless explicitly documented otherwise, functions that return `MARU_Status` must be called from the context's owner thread.
- Functions that do not return `MARU_Status` and access context-owned state can be called from arbitrary threads, provided you synchronize with the owner thread (e.g., with a R/W lock).
- `maru_postEvent()`, `maru_wakeContext()`, `maru_*retain()`, `maru_*release()`, and `maru_getVersion()` are internally synchronized and can be called from any thread without external synchronization.

#### Aren't there mutations that Maru could flag as thread-safe-with-external-synchronization? 

Quite possibly, but we have no intention of getting into that. 

Going for that level of granularity would make the threading model more complex for you to reason about, and the implementation a lot harder for us to maintain reliably. The current contract is easier to work with and more robust in the long term.

### Does Maru use X11 or Wayland on Linux?

Yes!

You use either or both. The default is a one-size-fits-all mode that will try Wayland first and fall back to X11 if it cannot use it, but you can force it one way or another at runtime. Every system dependency is manually loaded and checked, so your game/app will work as long as either X11 or Wayland is available.

If neither of them is available, context creation will fail at runtime.

If you want to exclusively support X11 or Wayland, that's also possible, and it eliminates every shred of overhead from the dynamic selection machinery (what little there is). Just link against the `maru::x11` or `maru::wayland` target instead of `maru::maru`.

### How does Maru handle high-frequency mouse input?

By default, every single mouse movement is dispatched to your callback to ensure zero-latency access to raw OS input. However, Maru provides a `MARU_Queue` utility that supports opt-in **event coalescence**. It can combine high-frequency events (like mouse motion or scrolling) into a single event with accumulated deltas while strictly preserving temporal ordering with other events. The queue also provides sane and efficient multi-threaded event processing.

See the [Event Queues documentation](docs/user/queue.md) for more details.

### Does Maru initialize Vulkan for me?

Nope. There are too many decisions to make around that. The library has a mandate: deal with windows and user I/O (not audio, filesystem, or networking), and it sticks to it. It does a grand total of 2 Vulkan-specific things:

- Get which extensions you need to provide `vkCreateInstance()` via `maru_getVkExtensions()`
- Create a `VkSurfaceKHR` (which you are responsible for deleting) for a given window via `maru_createVkSurface()`

### Can I use Maru for other rendering APIs (OpenGL, DX11/12, etc...)?

Not natively at the moment. Maybe one day, if there's enough demand for it. However, supporting OpenGL or other similar legacy APIs would involve breaking some of the hard guarantees that we are currently providing, so we are not keen on it. DX12/Metal would be an easier sell.

In the meantime, you can always reclaim the OS-specific window handles and bind them yourself.

### Is there really no synchronous window creation mechanism?

Unfortunately, asynchronous window creation is absolutely necessary to get a smooth experience in certain backends. On top of that, synchronous window creation is too sticky/tempting of an API, so it's easy to paint oneself into a corner and run into issues later down the road.

For a single-window app/game, as shown in the usage example, you can fairly easily work around that with a dedicated pump loop right after the window creation.

### Why do Controllers and Monitors use a retain() / release() reference counting system?

Because hardware is physically volatile, and your threads are not.

In a multi-threaded engine, your simulation thread might be actively reading from a MARU_Controller* at the exact millisecond the user violently unplugs it from their USB port. If Maru automatically freed the controller's memory upon receiving the OS disconnect event, your simulation thread would instantly segfault on a dangling pointer.

Instead, Maru uses reference counting. When you retain controller or monitor, you own a reference to it. If the device is physically disconnected, Maru flags the object as "lost" (which you can check via maru_isControllerLost() or maru_isMonitorLost()). It will stop receiving new data and API calls will safely no-op or fail, but the pointer remains valid in memory until you explicitly call maru_release*(), or the Context is destroyed.

This guarantees you will never suffer a use-after-free crash due to unpredictable hardware events. If you cache a pointer, retain it. When you drop it, release it.

### What's up with the attribute substructs in the createInfos?

Some properties of context/windows must be set at creation, and others can be changed on the fly later. Attributes represent the latter, and the same struct type is used when populating the create infos and when invoking `maru_updateWindow()` / `maru_updateContext()`. That makes things nice and consistent.

### Can I store event pointers for later?

**No.** The `MARU_Event*` pointer, as well as any data it points to, passed to your callback is only valid for the duration of that callback.

### What's the difference between logical vectors and pixel vectors?

The wording distinction is to make dealing with High DPI (retina) displays easier.

All coordinate variables and types have either `logical` or `pixel` in their name to make crystal clear if they are referring to logical OS dimensions or to the actual pixel count. In short, use `logical` units for your UI and `pixel` for rendering.

### Why do I need to pass callback and mask to maru_pumpEvents() every frame?

One of the things that's always bugged me about GLFW is that I am never fully confident *when* my callback is getting invoked. The only way I can be truly 100% sure my callback isn't called when I don't want it is simply to not let the library have it at all.

Maru performs direct, synchronous event dispatch. It pulls events from the OS and fires them inline, strictly during the pump. That is the only time your callback comes into play. Forcing you to provide the callback and mask to maru_pumpEvents() every frame enshrines this on both sides of the fence.

### How am I supposed to use MARU_TextEditCommitEvent correctly?

Yeah... The only text-input mechanism Maru provides is the full complicated IME payload, which can be a bit overwhelming. But Maru's philosophy is "what you build on day one will take you to the finish line", so it forces you to eat your veggies.

What you should **NOT** do is use `MARU_KeyboardEvent` as a quick-and-dirty stop-gap.
- It does not respect keyboard layouts.
- It does not produce key repeats.
- It does not handle internationalization.

Instead, use the `maru_applyTextEditCommitUtf8()` convenience function that will apply a text commit to a string buffer in-place.

If you also take the time to update `MARU_WINDOW_ATTR_TEXT_INPUT_RECT` so the OS knows where to draw the IME candidate popup over your game/app, you'll have proper Japanese/Korean/etc... support pretty much out-of-the-box.

### Why does Maru create a worker thread in some backends? I thought this was supposed to be lightweight!

Maru isn't lightweight for the sake of it. It is lightweight so that your app/game can run as fast and smoothly as possible.

Certain OS operations (e.g. scanning for gamepads on Linux via udev or handling device-change notifications on Windows) can block for several milliseconds. If we ran these on your main thread, you could see a hitch in your frame rate every time a controller was plugged in.

We offload these non-deterministic blocking calls to a dedicated worker thread. This ensures that maru_pumpEvents() remains fast and predictable, even during unusual OS events.

That's why having an extra thread (in the backends that warrant it) is the lightweight thing to do.

### Do I really need CMake to build the library itself? Can I just compile the files myself and use the headers?

Not reliably. Even if it was reasonably feasible today, it might break with any release. This is mostly because the X11 and Wayland backends each need to be compiled twice (direct use vs runtime-dispatched). The CMake path is the only one we plan on ensuring stability on for now. If you want to integrate Maru in some other build system, headers + precompiled library is the way to go.

## Acknowledgements

While there has been a lot of divergence since, a lot of the design and code was originally inspired by, if not at times lifted from, the outstanding [GLFW](https://glfw.org). At the time of writing, credit goes to Marcus Geelnard (2002-2006) and Camilla Löwy (2006-2019)

The examples' code is derived from [Vulkan Tutorial](https://github.com/Overv/VulkanTutorial), but with modifications.

## License

Maru is licensed under the Zlib license. See [LICENSE.txt](LICENSE.txt) for details.
