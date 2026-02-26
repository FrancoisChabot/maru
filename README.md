# MARU

[![License](https://img.shields.io/badge/license-zlib-blue.svg)](LICENSE.md)

A lightweight vulkan-first Platform Abstraction Layer for the modern age.

What's neat about Maru?

- It has no dynamic global state whatsoever, and is completely inert until invoked.
- It goes out of its way to provide fast and steady timing (< 0.1ms generally, 0.3ms with a high-frequency mouse), even during unusual events like a controller hot-plugs.
- API use is aggressively validated in debug, and all guardrails can be disabled for maximum performance.
- It exposes clear and detailed metrics about its internal timing and resource usage.

## Status

Ready for use by early adopters on Linux.

## Quickstart 

Run the following, and you should be presented with a ImGui-based playground to mess with the various things Maru provides.

You first need need: 
  - A C compiler (gcc/clang/msvc)
  - [git](https://git-scm.com/)
  - [CMake](https://cmake.org/) (3.20 or above)
  - The [Vulkan SDK](https://vulkan.lunarg.com/sdk/home).

```bash 
# Linux/macOS: you may or may not have to do this depending on your environment.
# source path/to/vulkan/sdk/setup-env.sh

git clone https://github.com/birdsafe/maru
cd maru
cmake -S . -B build
cmake --build build --config Release
# On macOS/Linux
./build/examples/maru_testbed
# On Windows
#./build/examples/Release/maru_testbed
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

## Safety, Validation and diagnostics

Maru provides a few different options to control the validation behavior. These are all fixed at the time of compiling the library.

* **MARU_VALIDATE_API_CALLS**: `OFF` by default. Setting it to `ON` will activate VERY aggressive validation of the API use. This includes thread affinity checks, state validation, the works.

* **MARU_ENABLE_DIAGNOSTICS**: `ON` by default. Setting it to `OFF` will strip virtually all logging and reporting overhead from the library, including the string literals.

* **MARU_GATHER_METRICS**: `ON` by default. Setting it to `OFF` it will stop the gathering of internal metrics.

* **MARU_ENABLE_INTERNAL_CHECKS**: `OFF` by default. Setting it to `ON` will enable a lot of internal validation logic. (Only use this if you suspect a bug in Maru itself).

### Recommended configurations

| Build Type         | `MARU_VALIDATE_API_CALLS` | `MARU_ENABLE_DIAGNOSTICS` | `MARU_GATHER_METRICS` | `MARU_ENABLE_INTERNAL_CHECKS` |
| :---               | :---:                     | :---:                     | :---:                 | :---:                         |
| **Development**    | `ON`                      | `ON`                      | `ON`                  | `OFF`                         |
| **Release**        | `OFF`                     | `ON`                      | `ON`                  | `OFF`                         |
| **Production**     | `OFF`                     | `OFF`                     | `OFF`                 | `OFF`                         |
| **Debugging Maru** | `ON`                      | `ON`                      | `ON`                  | `ON`                          |

**N.B.** `MARU_ENABLE_INTERNAL_CHECKS` can have subtle implications beyond performance in some cases. See [Internal Checks Caveats](docs/dev_guide/internal_checks_caveats.md) for details.


## FAQs

### Is Maru Thread-safe?

Here are the rule of thumbs:

- Each `MARU_Context` is its own little universe. They are fully independant from one-another, and you can instantiate different contexts in different threads.
- The thread that creates a `MARU_Context` is that context's owner thread.
- `maru_postEvent()`, `maru_wakeContext()`, `maru_*retain()` and `maru_*release()` are the only functions that are truly thread-safe.
- Any function that does not return a `MARU_Status` is a **pasive accessor**, and can be safely called from arbitrary threads; provided that you synchronize with
the Context's thread with a R/W lock.
- Any other function has to be called from the context's owner thread.

### Does Maru use X11 or Wayland on Linux?

Yes!

You use either or both. The default is a one-size-fits-all mode that will try Wayland first and fall back to X11 if it cannot use it, but you can force it one way or another at runtime. Every system dependency is manually loaded and checked, so your game/app will work as long as either X11 or Wayland is available.

If you want to exclusively support X11 or Wayland, that's also possible, and it eliminates every shred of overhead from the dynamic selection machinery (what little there is). See the [Integration guide](docs/user_guide/integration.md) for details.

### How does Maru handle high-frequency mouse input?

The short answer is that it doesn't by default. Every single mouse movement will be dispatched to your callback to accomodate engiens that already provide their own event queues. However, Maru also provides a high-performance queue that can easily be integrated and performs efficient order-respecting event coalescense for you.

### Does Maru initialize Vulkan for me?

Nope. There are too many decisions to make around that. The library has a mandate: deal with windows and I/O, and it sticks to it. It does a grand total of 2 Vulkan-specific things:

1 - Get which extensions you need to provide `vkCreateInstance()` via `maru_vulkan_getVkExtensions()`
2 - Create a `vkSurfaceKHR` for a given window (which you are responsible for deleting) via `maru_vulkan_createVkSurface()`

### Is there really no synchronous window creation mechanism?

"You'll thank me later." is a crass way to put it, but that's effectively our stance on this.

Unfortunately, asynchronous window creation is absolutely necessary to get a smooth experience in certain backends. On top of that, synchronous window creation is too sticky/tempting of an API, so it's easy to paint oneself in a corner and run into issues later down the road.

### What's up with the attribute substructs in the createInfos? What goes in them seems arbitrary.

Some properties of context/windows must be set at creation, and others can be changed on the fly later. Attributes represent the later, and the same struct type is used when populating the create infos and when invoking `maru_updateWindow()` / `maru_updateContext()`. That makes things nice and consistent.

### Can I store event pointers for later?

**No.** The `MARU_Event*` pointer passed to your callback is only valid for the duration of that callback.

### What's the difference between logical vectors and pixel vectors

The wording distinction is to make dealing with High DPI (retina) displays easier.

All coordinate variables and types have either `logical` or `pixel` in their name to make crystal clear if they are referring to logical OS dimensions or to the actual pixel count. In short, use `logical` units for your UI and `pixel` for rendering.

### Why do I need to pass the event handling callback to maru_pumpEvents().

Maru guarantees that events are only ever dispatched during maru_pumpEvents(). Having the callback only exist for the during of the pump enshrines this on both sides of the fence.

### Why does Maru create a worker thread (in some backends)? I thought this was supposed to be light!

There are a few interactions (e.g. hot-plugging a controller in Linux) that inevitably introduce timing unpredictability on certain platforms. These operations are offloaded to the worker thread so that your app/game doesn't get stuck for up to 20ms when they happen.

### Do I really need CMake to build the library itself? Can I just compile the files myself and use the headers?

Not reliably. Even if it was reasonably feasible today, it might break with any release. The CMake path is the only one we plan on ensuring stability on.

## Acknowledgements

While there has been a lot of divergence since, a lot of the design and code was originally inspired by, if not at times lifted from, the outstanding [GLFW](https://glfw.org). At the time of writing, credit goes to Marcus Geelnard (2002-2006) and Camilla Löwy (2006-2019)

The examples' code are derived from [Vulkan Tutorial](https://github.com/Overv/VulkanTutorial), but with modifications.

## License

Maru is licensed under the Zlib license. See [LICENSE.md](LICENSE.md) for details.
