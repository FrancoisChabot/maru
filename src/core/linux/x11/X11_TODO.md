# X11 Backend TODO

This document tracks the remaining work and identified improvements for the Maru X11 backend.

## Priority 1: Correctness & Essential Features

- [x] **Incremental Selection Transfer (INCR)**:
  - [x] Implemented **receive-side** INCR handling in `x11_dataexchange.c` for
    `maru_requestData()` (`SelectionNotify` INCR handshake + `PropertyNotify`
    chunk accumulation + zero-length termination).
  - [x] Implement **send-side** INCR in `maru_provideData_X11()` for payloads
    larger than the X server request/property size limits.
  - Required for transferring data (clipboard/DnD) larger than the maximum
    request size supported by the X server.
- [x] **Window State Synchronization**:
  - `PropertyNotify` for `_NET_WM_STATE`/`WM_STATE` now reconciles window-managed state in the event loop.
  - Internal flags (`MARU_WINDOW_STATE_MAXIMIZED`, `MARU_WINDOW_STATE_FULLSCREEN`, minimized via `WM_STATE`) are updated from WM properties and presentation state change events are dispatched when `maximized`/`minimized` bits change.
- [x] **Visibility Synchronization**:
  - `UnmapNotify` and `MapNotify` now reconcile `MARU_WINDOW_STATE_VISIBLE` in `x11_window.c`.
  - `MARU_EVENT_WINDOW_PRESENTATION_STATE_CHANGED` is dispatched when visibility changes due to system-driven map/unmap transitions.
- [x] **Frame Timing (`_NET_WM_FRAME_DRAWN`)**:
  - Implement the EWMH frame synchronization protocol.
  - Allows `maru_requestWindowFrame()` to be throttled by the compositor, preventing over-rendering and providing smooth animations.

## Priority 2: Modern Desktop Experience

- [x] **High-DPI and Scaling Support**:
  - Added DPI detection in X11 via `Xft.dpi` from `RESOURCE_MANAGER`, with physical-dimension fallback.
  - `maru_getWindowGeometry_X11()` now reports DPI-aware `scale`, `logical_size` (DIP), and `pixel_size`.
  - Monitor refresh now reports DPI-aware `scale` and DIP-based logical monitor position/size.
- [x] **Idle Inhibition**:
  - Replaced the stub in `maru_updateContext_X11()` with a real implementation.
  - Added `MIT-SCREEN-SAVER` integration via `XScreenSaverSuspend` (loaded from `libXss.so.1` when available) and context lifecycle handling to release inhibition on destroy.

## Priority 3: Enhanced Input & Polish

- [x] **XI2 Pointer Barriers**:
  - `MARU_CURSOR_LOCKED` now uses XFixes pointer barriers when XInput 2.3+ and XFixes 5+ are available, constraining the pointer to a tiny center box while motion is driven from XI2 raw events.
  - The previous `XWarpPointer` path remains as a compatibility fallback when pointer barriers are unavailable, and is still used for the initial recenter / resize recenter operations.
- [ ] **Input Consolidation**:
  - Migrate all mouse and keyboard event processing to XInput2 for better consistency and high-precision data (e.g., sub-pixel scrolling).
- [x] **File Naming Consistency**:
  - Renamed `X11_context.c` to `x11_context.c` to match the lowercase naming convention of other backend files.
