# Miscellaneous Behaviors

This document tracks cross-platform behavioral requirements and invariants that are not explicitly part of the core API definitions but are expected of all backends.

## 1. Window Management

### 1.1 Cursor Destruction
When a `MARU_Cursor` is destroyed, any window currently using it as its `current_cursor` MUST:
1. Set its `current_cursor` to `NULL`.
2. Revert to the system default cursor.
3. Update its `attrs_requested.cursor` and `attrs_effective.cursor` to `NULL`.
4. If the cursor is currently active (e.g., the pointer is over the window in `MARU_CURSOR_NORMAL` mode), the change should be reflected immediately.
