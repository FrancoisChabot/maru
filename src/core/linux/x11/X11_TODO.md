# X11 Backend TODO

This document tracks the remaining work and identified improvements for the Maru X11 backend.

## Priority 1: Correctness & Essential Features

- [ ] **Incremental Selection Transfer (INCR)**:
  - Implement the X11 `INCR` protocol in `x11_dataexchange.c`.
  - Required for transferring data (clipboard/DnD) larger than the maximum request size supported by the X server.
- [x] **Window State Synchronization**:
  - `PropertyNotify` for `_NET_WM_STATE`/`WM_STATE` now reconciles window-managed state in the event loop.
  - Internal flags (`MARU_WINDOW_STATE_MAXIMIZED`, `MARU_WINDOW_STATE_FULLSCREEN`, minimized via `WM_STATE`) are updated from WM properties and presentation state change events are dispatched when `maximized`/`minimized` bits change.
- [x] **Visibility Synchronization**:
  - `UnmapNotify` and `MapNotify` now reconcile `MARU_WINDOW_STATE_VISIBLE` in `x11_window.c`.
  - `MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED` is dispatched when visibility changes due to system-driven map/unmap transitions.
- [ ] **Frame Timing (`_NET_WM_FRAME_DRAWN`)**:
  - Implement the EWMH frame synchronization protocol.
  - Allows `maru_requestWindowFrame()` to be throttled by the compositor, preventing over-rendering and providing smooth animations.

## Priority 2: Modern Desktop Experience

- [ ] **High-DPI and Scaling Support**:
  - Implement DPI detection (via `Xft.dpi` X resource or physical screen dimensions).
  - Report correct scale factors in `maru_getWindowGeometry_X11()` and monitor info.
  - Support `_NET_WM_WINDOW_OPACITY` for per-window transparency.
- [ ] **Idle Inhibition**:
  - Replace the stub in `maru_updateContext_X11()` with a real implementation.
  - Support the `MIT-SCREEN-SAVER` extension or D-Bus `org.freedesktop.ScreenSaver` integration.

## Priority 3: Enhanced Input & Polish

- [ ] **XI2 Pointer Barriers**:
  - Utilize `XI_Barrier` (XInput 2.3+) for the `MARU_CURSOR_LOCKED` mode.
  - Provides a more robust alternative to the current `XWarpPointer` approach, which can be jittery or bypassable in some compositors.
- [ ] **Multi-touch and Gestures**:
  - Add support for XInput2 touch events (`XI_TouchBegin`, `XI_TouchUpdate`, etc.) if Maru adds a generic touch API.
- [ ] **Input Consolidation**:
  - Migrate all mouse and keyboard event processing to XInput2 for better consistency and high-precision data (e.g., sub-pixel scrolling).
- [x] **File Naming Consistency**:
  - Renamed `X11_context.c` to `x11_context.c` to match the lowercase naming convention of other backend files.
