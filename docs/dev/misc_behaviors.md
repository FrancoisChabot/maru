# Miscellaneous Behaviors

This document tracks implementation-specific behaviors that are not explicitly part of the core API but are essential for a consistent user experience.

## 1. Cursor Lifecycle

### Automatic Reversion on Destruction
When a custom `MARU_Cursor` is destroyed via `maru_destroyCursor()`, any `MARU_Window` currently using that cursor MUST automatically revert its cursor to the system default (`MARU_CURSOR_SHAPE_DEFAULT`).

**Rationale:**
Allowing a window to maintain a handle to a destroyed resource is unsafe and can lead to unpredictable OS-level behavior or dangling pointers in the library's internal state. Reverting to a known-safe state (the system arrow) ensures stability and visual consistency.
