# API Contracts

This page summarizes the user-visible API contracts that Maru enforces in
validation builds.

The implementation source of truth lives in the private header
`src/core/maru_api_constraints.h`. This document is intentionally not a
line-by-line mirror of that file. It captures the stable rules that matter to
users and should be audited against the private constraints header before a
release.

## Validation Behavior

When `MARU_VALIDATE_API_CALLS` is enabled, invalid API usage is a contract
violation and Maru will abort().

You should follow these rules in all builds, not just validation builds.
Validation builds only make contract violations obvious.

Context loss is not an API violation. Once a context is lost, most
`MARU_Status` APIs will short-circuit with `MARU_CONTEXT_LOST` until the
context is destroyed.

## Threading Contracts

- The thread that creates a `MARU_Context` is the owner thread.
- Unless a function is explicitly documented otherwise, `MARU_Status` APIs are
  owner-thread APIs.
- Direct-return state access APIs, polling getters, native-handle getters, and
  userdata accessors may be called from other threads, but only with external
  synchronization against owner-thread operations on the same handle or
  context.
- `maru_postEvent()` and `maru_wakeContext()` are globally thread-safe and
  return `bool`.
- `maru_retainMonitor()`, `maru_releaseMonitor()`, `maru_retainController()`,
  and `maru_releaseController()` are globally thread-safe.
- A `MARU_Queue` has its own creator thread. Except for `maru_scanQueue()`,
  all queue APIs are creator-thread APIs.
- `maru_scanQueue()` is globally thread-safe and may be called from any
  thread, but must not race with `maru_commitQueue()` on the same queue.

## Lifetime And Liveness

- Transient lists and event-backed pointers are generally valid only until the
  next call to `maru_pumpEvents()`.
- Monitor and controller handles obtained from lists or events are transient.
  Retain them if you need to store them beyond the current pump cycle.
- Drop-session handles are callback-scoped. They are valid only for the
  duration of the DnD callback that delivered them and must not be stored.
- Drop-session userdata persists across callbacks for the same logical drag
  session, but you recover it through the callback-scoped handle delivered to
  each callback.
- A window handle may exist before it is ready. Operations that need the native
  window to exist must wait for `MARU_EVENT_WINDOW_READY` or
  `maru_isWindowReady(window)`.
- Lost monitor and controller handles remain valid in memory until you release
  them. Snapshot-style accessors and getters continue to expose the last known
  state, which may be stale after loss. Backend-facing mutators become inert
  and return `MARU_FAILURE`.
- `surrounding_text` is either `NULL` with zero offsets, or valid
  NUL-terminated UTF-8 with cursor/anchor byte offsets that are in-bounds and
  land on UTF-8 code point boundaries.
- Text-edit event byte ranges and deletion spans are reported in UTF-8 byte
  units and always align to UTF-8 code point boundaries.
- `MARU_UserDefinedEvent.raw_payload` starts at the same address as `userdata`
  and therefore has at least pointer alignment. If you reinterpret it as some
  other type, that type must fit in 64 bytes and must not require stricter
  alignment than `void*`.
- Validation builds require these window operations to target a ready window:
  - `maru_updateWindow()`
  - `maru_requestWindowFocus()`
  - `maru_requestWindowFrame()`
  - `maru_requestWindowAttention()`
  - drag-and-drop data exchange APIs on a window
  - native window handle getters
  - `maru_createVkSurface()` from `maru/vulkan.h`

## Cross-Context Ownership

- Handles passed together must belong to the same `MARU_Context` when the API
  logically ties them together.
- In window attributes, `monitor`, `cursor`, and `icon` must belong to the same
  context as the target window.
- Custom cursor frame images must belong to the same context as the cursor
  being created.
- `maru_postEvent()` posts context-scoped user events only. Callbacks for those
  events always receive `window == NULL`.
- The first `MARU_MOUSE_DEFAULT_COUNT` mouse button channels are always present
  and always correspond to `MARU_MouseDefaultButton` in enum order. Extra mouse
  button channels, if any, follow after that range.
- For controllers where `maru_isControllerStandardized(controller)` returns
  `true`, the first `MARU_CONTROLLER_ANALOG_STANDARD_COUNT`,
  `MARU_CONTROLLER_BUTTON_STANDARD_COUNT`, and
  `MARU_CONTROLLER_HAPTIC_STANDARD_COUNT` channels are always the Maru-defined
  standardized channels in enum order. Extra channels, if any, follow after
  that range.
- `MARU_ChannelInfo.min_value` / `max_value` describe the inclusive public
  range for that channel. Button channels use 0..1. Standard stick axes use
  -1..1. Standard triggers and haptics use 0..1.

## Value And Mask Rules

- Any `field_mask` argument may only contain documented bits for that API.
- Enum-like parameters must be within their documented ranges.
- Pointers required by the API must be non-null.
- String parameters such as titles and MIME types must be non-null. MIME types
  must also be non-empty.
- Logical sizes, min sizes, max sizes, viewport sizes, and rectangle sizes must
  be non-negative.
- Image sizes must be strictly positive.
- `MARU_ImageCreateInfo.pixels` must be non-null.
- Monitor video modes passed to `maru_setMonitorMode()` must have strictly
  positive pixel dimensions.
- `maru_setControllerHapticLevels()` requires every intensity to fall within
  the published inclusive range for the targeted haptic channels.
- `MARU_QueueCreateInfo.capacity` must be greater than zero.
- `MARU_QueueCreateInfo.allocator` uses the default allocator only when all
  allocator callbacks are null. Custom allocators are all-or-none.

## Special Value Semantics

- `MARU_WindowAttributes.aspect_ratio` must be either `{0, 0}` to disable the
  constraint, or a fraction with both `num` and `denom` greater than zero.
- Window max size uses `0` as an unbounded sentinel on each axis. When both min
  and max size are supplied together, each nonzero max axis must be greater
  than or equal to the corresponding min axis.
- Window presentation state requests are exclusive: `fullscreen`, `maximized`,
  and `minimized` may not overlap. `fullscreen` and `maximized` require
  `visible == true`; `minimized` requires `visible == false`; a hidden
  non-minimized window uses `visible == false` with the others false.
- `MARU_ImageCreateInfo.stride_bytes` may be `0` for the default tightly packed
  layout. If nonzero, it must be at least `width * 4` bytes.
- `maru_pumpEvents()` and `maru_scanQueue()` require a non-null callback when
  `mask != 0`. When `mask == 0`, the callback may be `NULL`.

## API-Specific Notes

### Cursors

- `MARU_CursorCreateInfo.source` must be either custom or system.
- A custom cursor must provide at least one frame and a non-null frame array.
- Every custom cursor frame must provide a non-null image.
- A custom cursor hotspot must be inside the referenced image bounds.
- A system cursor must use a valid `MARU_CursorShape`.
- `maru_createCursor()` fails if the active backend cannot provide the
  requested system cursor shape. Maru does not substitute a fallback cursor.

### Data Exchange

- `MARU_DataExchangeTarget` must be either
  `MARU_DATA_EXCHANGE_TARGET_CLIPBOARD` or
  `MARU_DATA_EXCHANGE_TARGET_DRAG_DROP`.
- The allowed drop action mask may only contain `COPY`, `MOVE`, and `LINK`.
- `maru_setDropSessionAction()` accepts only `NONE`, `COPY`, `MOVE`, or
  `LINK`. Multi-bit combinations are invalid, and any non-`NONE` action must
  be present in the callback's `available_actions` mask.
- `maru_provideClipboardData()` and `maru_provideDropData()` require
  `data != NULL` when `size > 0`.
- `maru_provideClipboardData()` and `maru_provideDropData()` flags may only
  contain documented bits.
- `maru_setDropSessionAction()` is only meaningful during
  `MARU_EVENT_DROP_ENTERED` and `MARU_EVENT_DROP_HOVERED`.

### Queue

- Only queue-safe event ids may be pushed into a `MARU_Queue`. Use
  `MARU_QUEUE_SAFE_EVENT_MASK` or `maru_isQueueSafeEventId()` when capturing
  events from `maru_pumpEvents()`.
- Queue replay preserves only the copied event payload and the stable
  `MARU_WindowId` captured at enqueue time. It does not preserve a live
  `MARU_Window*` handle.
- Queue coalescing applies only to events with the same `(type, window_id)`
  pair.

### Native Handles And Vulkan

- Native context handle getters require a context whose backend matches the
  getter.
- Native window handle getters require a matching backend and a ready window.
- Vulkan helper declarations live in `maru/vulkan.h`.
- `maru_createVkSurface()` requires non-null `window`, `instance`, `vk_loader`,
  and `out_surface`, and must be called from the owner thread.

If you are unsure whether a call is safe before the window is ready, assume it
is not unless the API is documented as a direct-return cached state accessor.
