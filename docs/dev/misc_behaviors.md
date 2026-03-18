# Miscellaneous Behaviors

This document tracks cross-platform behavioral requirements and invariants that are not explicitly part of the core API definitions but are expected of all backends.

## 1. Window Management

### 1.1 Cursor Destruction
When a `MARU_Cursor` is destroyed, any window currently using it as its `current_cursor` MUST:
1. Set its `current_cursor` to `NULL`.
2. Revert to the system default cursor.
3. Update its `attrs_requested.cursor` and `attrs_effective.cursor` to `NULL`.
4. If the cursor is currently active (e.g., the pointer is over the window in `MARU_CURSOR_NORMAL` mode), the change should be reflected immediately.

## 2. Cursors & Images

### 2.1 Unsupported System Shapes
Backends MUST NOT synthesize fallbacks for system cursor shapes. If a backend cannot provide the requested native/system cursor shape defined in `MARU_CursorShape`, `maru_createCursor()` MUST return `MARU_FAILURE` and SHOULD report a `MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED` diagnostic.

## 3. Data Exchange

### 2.1 Additional Data Channels
The public C API currently exposes only `CLIPBOARD` and `DRAG_DROP` data exchange targets.

`PRIMARY` selection support was intentionally removed instead of being kept as a one-off Linux-skewed target. If a future product requirement brings back an additional selection/data channel, the correct direction is an open-ended data-channel model rather than another hard-coded enum slot in `MARU_DataExchangeTarget`.
