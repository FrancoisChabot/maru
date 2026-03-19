# Comprehensive MARU Test Plan

This document defines the expected test coverage for the MARU library across
the public API surface. It is intentionally broader than the current test
suite: it captures both what already exists and the gaps that still need
coverage before release confidence is high on Windows, macOS, X11, and
Wayland.

The suite should be split across four lanes:

1. `unit` for deterministic helpers, value contracts, queue semantics, and
   payload-copy rules.
2. `desktop_integration` for real-context / real-window lifecycle tests that
   can still run unattended.
3. `desktop_smoke` for presentation and rendering paths that need a real
   desktop or lightweight compositor.
4. `manual_testbed` for compositor-, OS-, and hardware-driven behaviors that
   cannot be asserted reliably in CI.

## 1. Automated Testing

Automated coverage should not be treated as only "smoke tests". MARU has a
large contract surface that deserves unit, integration, and validation-build
coverage.

### Context & Initialization

- **Context Lifecycle:** Create and destroy contexts repeatedly, with default
  allocators and custom allocators, and replay context creation under forced
  allocation failure.
- **Backend Selection:** Request explicit backends and verify success, failure,
  or fallback semantics are correct for the current platform.
- **Context Attributes:** Cover `diagnostic_cb`, `inhibit_idle`, and
  `idle_timeout_ms` both at create-time and via `maru_updateContext()`.
- **Wake / Post Semantics:** Verify `maru_wakeContext()` wakes a blocking pump
  and `maru_postEvent()` delivers only on a later pump cycle, never
  reentrantly.
- **User Event Queue Limits:** Add tests for disabled user-event queues,
  full-queue behavior, and recovery after the queue drains.
- **Pump Edge Cases:** Cover `timeout_ms = 0`, finite waits, `MARU_NEVER`,
  `mask = 0`, callback-null behavior when `mask = 0`, and event-mask
  filtering.
- **Context Loss:** Add targeted tests for the public `MARU_CONTEXT_LOST`
  contract so child-handle APIs become inert while `maru_destroyContext()`
  still succeeds.

### API Contracts & Validation Builds

- **Validation-Only Misuse Tests:** Add a dedicated validation-build lane that
  intentionally triggers contract violations in subprocesses and asserts
  abort/failure for:
  - invalid field masks
  - invalid enum values
  - null required pointers
  - negative or otherwise invalid geometry
  - partial custom allocators
  - cross-context handle misuse
  - pre-ready window API misuse
- **Threading Contracts:** Add coverage for APIs documented as globally
  thread-safe (`maru_postEvent()`, `maru_wakeContext()`, retain/release APIs,
  queue scanning) and explicitly avoid relying on undefined cross-thread calls.
- **Lifetime Contracts:** Verify borrowed lists and event-backed pointers are
  copied when persistence is required and are not accidentally reused after the
  documented lifetime.

### Window Management

- **Creation & Destruction:** Create windows with default, specific, and
  edge-case attributes. Destroy them immediately, after pumping events, and
  implicitly via context destruction.
- **Ready-State Semantics:** Verify which operations are legal before
  `MARU_EVENT_WINDOW_READY` and which correctly require a ready window.
- **Window Lookup:** Cover stable `MARU_WindowId` behavior across multiple live
  windows and failure on dead / unknown IDs.
- **Programmatic State Changes:** Exercise Normal, Hidden, Minimized,
  Maximized, and Fullscreen transitions, including transitions that require
  coordinated `visible` + `presentation_state` updates.
- **Window Attributes:** Cover set/get semantics for:
  - title
  - dip size
  - dip viewport size
  - dip position
  - monitor targeting
  - min/max size
  - aspect ratio
  - resizable
  - accept-drop
  - cursor mode / cursor handle
  - icon
  - text input type
  - text input rect
  - surrounding text / cursor / anchor bytes
- **Attention & Frame Requests:** Add explicit tests for
  `maru_requestWindowFocus()`, `maru_requestWindowFrame()`, and
  `maru_requestWindowAttention()` success/failure paths.
- **Multiple Windows:** Create multiple windows per context and ensure events,
  IDs, geometry, and teardown remain correctly scoped.

### Window Event Semantics

- **State Event Payloads:** Verify `MARU_EVENT_WINDOW_STATE_CHANGED` field masks
  and payload values match the corresponding accessors.
- **Resize / State Ordering:** Add checks for ordering between
  `WINDOW_READY`, `WINDOW_RESIZED`, `WINDOW_FRAME`, and
  `WINDOW_STATE_CHANGED`, especially around maximize/minimize/fullscreen
  transitions.
- **Close Request Handling:** Verify close requests stay window-scoped and do
  not implicitly destroy windows.
- **Frame Pacing:** Keep backend-specific tests such as X11 frame throttling
  and add equivalent smoke assertions where other backends expose meaningful
  frame callbacks.

### Input, Text, And IME

- **Keyboard Events:** Cover representative key press/release paths, modifier
  combinations, and repeat behavior where the backend exposes it.
- **Mouse Events:** Cover motion, raw delta, button changes, scrolling, and
  default-button metadata.
- **Polling Parity:** Add tests that correlate event delivery with polling
  accessors (`maru_getKeyboardKeyStates()`, mouse button state queries,
  controller polling).
- **Cursor Modes:** Add automated checks for normal/hidden/locked state
  transitions and no-crash behavior on unsupported backends.
- **Text Helpers:** Keep unit coverage for UTF-8 edit helpers and add window API
  tests for `text_input_type`, `dip_text_input_rect`, and surrounding-text
  updates.
- **IME Session Semantics:** Add integration coverage for start/update/commit/
  navigation/end ordering where backend automation is feasible; keep the rest
  manual.

### Monitors & Displays

- **Enumeration:** Verify monitor lists are stable for one pump cycle and that
  names, geometry, scale, and physical size accessors remain sane.
- **Retain / Release:** Add tests for retaining monitor handles across pump
  cycles and for last-known snapshot behavior after monitor loss.
- **Modes:** Query supported modes, validate current mode snapshots, and test
  `maru_setMonitorMode()` success/failure paths where safe.
- **Topology Events:** Add coverage for `MARU_EVENT_MONITOR_CHANGED` and
  `MARU_EVENT_MONITOR_MODE_CHANGED` payload correctness.

### Resource Lifecycle

- **Images:** Cover creation with tight and custom stride values, userdata, and
  destruction while attached as a window icon.
- **Cursors:** Cover system and custom cursors, single-frame and animated
  cursors, unsupported system-shape failure, and destruction while a cursor is
  active on a window.
- **Controllers:** Verify the no-controller case, enumeration snapshots,
  retain/release behavior, standardized channel ordering, published channel
  ranges, and haptic range validation.

### Data Exchange

- **Clipboard Ownership:** Test announce / clear ownership flows, MIME list
  enumeration, request / provide success paths, and empty-offer behavior.
- **DnD API Contracts:** Add automated tests for allowed-action masks,
  selected-action rules, drop-session userdata, and request/provide validation.
- **Payload Lifetime:** Add coverage for copied payloads versus
  `MARU_DATA_PROVIDE_FLAG_ZERO_COPY`, including `MARU_EVENT_DATA_RELEASED`.
- **Snapshot Invalidation:** Verify `maru_getAvailableClipboardMIMETypes()` and
  `maru_getAvailableDropMIMETypes()` return borrowed snapshots with the
  documented invalidation behavior.
- **Large Payload Paths:** Keep explicit coverage for X11 incremental clipboard
  transfers and extend it to other backends where large transfers have unique
  code paths.

### Vulkan & Native Handles

- **Extension Enumeration:** Verify `maru_getVkExtensions()` returns the
  backend-appropriate extension set and stable borrowed strings.
- **Surface Creation:** Create Vulkan surfaces only after readiness and verify
  creation fails cleanly when preconditions are violated.
- **Native Handle Getters:** Add backend-specific tests for X11, Wayland,
  Win32, and Cocoa handle getters on matching and mismatched backends, plus
  ready-window requirements for window handles.

### Queue Surface

- **C Queue API:** Keep current coverage for push/commit/scan, masking,
  coalescing, overflow compaction, custom allocators, and diagnostics.
- **Queue Event Safety:** Add explicit assertions that queue replay preserves
  copied payloads and stable window IDs only, not live window handles.
- **Threading:** Add integration tests for the documented scan-vs-commit
  synchronization model.
- **C++ Queue API:** Extend the existing C++ queue tests beyond the happy path
  to cover RAII teardown, visitor mask inference, and failure propagation.

### C++ Wrapper Surface

- **RAII Handles:** Add targeted tests for `maru::Context`, `maru::Window`,
  `maru::Queue`, and wrapper error propagation through `expected`-style
  results.
- **Parity Checks:** Verify wrapper helpers preserve the same ready-window,
  queue-safe, and lifetime contracts as the C API.

## 2. Human-Driven Testing (Testbed Application)

The testbed in `examples/testbed/` is already the right manual-testing entry
point. It already contains usable modules for:

- event masks and idle timeout control
- event logging
- instrumentation / diagnostics
- windowing
- monitors
- input polling
- cursors
- controllers
- composition / IME
- data exchange

The remaining work is mostly refinement, not greenfield UI creation.

### Testbed Required Refinements

1. **Windowing refinements:** Add controls for visibility/minimize toggles,
   request-attention, dip position, dip viewport size, monitor targeting, and
   explicit text-input-related attributes.
2. **Monitor targeting workflow:** Add a UI path to move or fullscreen a window
   onto a selected monitor so backend-specific monitor semantics can be tested
   directly.
3. **Data exchange diagnostics:** Expose selected DnD action, zero-copy
   lifetime, and `DATA_RELEASED` events in the UI.
4. **IME controls:** Add UI for switching `MARU_TextInputType`, updating the
   text-input rectangle, and pushing surrounding-text tuples.
5. **Native/Vulkan diagnostics:** Add a lightweight panel that shows active
   backend type, native-handle availability, and Vulkan extension strings.

### Interactive Test Cases

#### Windowing & OS Integration

- **Decorations & Resizing:** Drag window borders to resize and verify rendered
  content adapts without artifacting or deadlock.
- **Moving & Positioning:** Move windows across the desktop and verify geometry
  updates stay coherent. On Wayland, confirm position requests fail or no-op in
  the documented manner rather than pretending to succeed.
- **Multi-Monitor Traversal:** Move windows across monitors with different DPI
  and verify scale / geometry updates.
- **OS State Changes:** Minimize, restore, maximize, fullscreen, hide, and
  request attention through both MARU APIs and OS window controls.
- **Secondary Windows:** Open multiple testbed windows, close them in different
  orders, and confirm event routing and teardown are isolated.

#### Input Devices

- **Keyboard:** Verify alphanumerics, modifiers, layout-sensitive keys, key
  repeat, and focus loss while keys are held.
- **Mouse / Pointer:** Verify left/right/middle/X1/X2, smooth and stepped
  scrolling, pointer enter/leave, raw motion, and locked-cursor behavior.
- **Text & IME:** Exercise a real OS input method and verify preedit,
  committed text, navigation commands, and cancellation semantics.
- **Controllers:** Plug and unplug a gamepad, verify connection events, analog
  range mapping, standardized button labels, and rumble stop/start behavior.

#### Data Exchange

- **Clipboard:** Copy text out of the testbed and paste text in from external
  apps. Repeat with large payloads.
- **Outgoing Drag:** Drag text from the testbed into another application and
  verify the selected action and final completion event.
- **Incoming Drop:** Drop files and text from external apps, verify MIME types,
  payload delivery, file-path reporting, and session-action feedback.

#### Monitors & Displays

- **Hotplug:** Connect or disconnect a monitor while the app is running and
  verify monitor events and refresh logic.
- **Mode Changes:** Switch display modes where safe and verify resulting mode
  and scale reporting.
- **Fullscreen Targeting:** Enter fullscreen on a selected monitor and verify
  the target monitor matches expectations.

#### Cursors

- **Visual Check:** Cycle through system shapes and confirm they match OS
  expectations or fail explicitly when unsupported.
- **Custom Image:** Apply custom and animated cursors and confirm hotspot
  placement, animation cadence, and fallback to default after destruction.
- **Cursor Modes:** Verify hidden and locked modes visually and behaviorally,
  including recovery on Escape in the testbed.

#### Performance & Robustness

- **Spam Input:** Shake the mouse, spam keys, and repeat clipboard / drag
  operations while watching for stalls or leaks.
- **Continuous Resizing:** Resize continuously and ensure the swapchain or
  present path recovers from out-of-date surfaces cleanly.
- **Suspend / Resume / Focus Churn:** Background the app, switch spaces/desktops
  if applicable, and return to verify event delivery and rendering resume
  cleanly.

## 3. Backend-Specific Exit Criteria

Each backend should not be considered "covered" until all of the following are
true:

- the automated unit / integration / smoke lanes pass for that backend
- backend-specific native-handle and Vulkan tests pass where applicable
- the manual testbed checklist has been executed at least once on real hardware
- known backend limitations are reflected in the corresponding user docs

## 4. Highest-Priority Gaps Today

Based on the current tree, the most obvious missing coverage is:

1. Validation-build misuse tests for API contracts.
2. Native-handle getter coverage across all backends.
3. Context-loss behavior tests.
4. Window attribute coverage for monitor targeting, text-input fields,
   surrounding text, viewport size, visibility/minimize interactions, and
   attention requests.
5. Data-exchange lifetime tests, especially zero-copy and `DATA_RELEASED`.
6. Monitor/controller retain-release and lost-handle semantics.
7. C++ wrapper parity tests beyond the queue happy path.
8. Testbed refinements for monitor targeting, text-input controls, and data
   exchange diagnostics.
