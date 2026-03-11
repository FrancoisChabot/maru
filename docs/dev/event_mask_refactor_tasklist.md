# Event Mask Refactor Tasklist

## Goal
Remove persistent context/window event masks and move filtering to pump-time APIs.

## Progress
- [x] 1. Add/confirm target API surface
  - `maru_pumpEvents(context, timeout_ms, mask, callback, userdata)`
  - `maru_queue_push(queue, type, window, event)`
- [x] 2. Remove context-level persistent event mask
  - Remove `MARU_CONTEXT_ATTR_EVENT_MASK`
  - Remove `MARU_ContextAttributes::event_mask`
  - Remove runtime storage/updates of context event mask
- [x] 3. Remove window-level persistent event mask
  - Remove `MARU_WINDOW_ATTR_EVENT_MASK`
  - Remove `MARU_WindowAttributes::event_mask`
  - Remove public accessor `maru_getWindowEventMask`
  - Remove runtime storage/updates of window event mask
- [x] 4. Update core dispatch path
  - Replace persistent filtering in `_maru_dispatch_event` with per-pump mask
  - Ensure queue/internal dispatch still works through pump-local filtering
- [x] 5. Update backend pump implementations and entry points
  - Windows, macOS, X11, Wayland, indirect backend vtable, exported entry files
- [x] 6. Update queue push model and docs
  - Remove queue-owned pumping/inline dispatch split
  - Add queue-safe event mask/helpers for callback-driven push
  - Validate queue-safe/thread rules under `MARU_VALIDATE_API_CALLS`
- [x] 7. Update C++ wrappers and convenience helpers
  - Context/Queue wrappers and any removed window mask helpers
- [x] 8. Update examples/tests/docs
  - Replace old pump signatures and removed mask APIs
- [x] 9. Build/test verification
  - Run targeted unit/integration tests touching queue/event APIs
  - Fix regressions

## Notes
- Keep callback syntax consistent across direct pump and queue capture callbacks.
- `cmake --build build -j8` succeeds.
- `ctest --test-dir build --output-on-failure`:
  - `maru_tests` passes.
  - `maru_integration_tests` fails in this environment because context creation is unavailable.
