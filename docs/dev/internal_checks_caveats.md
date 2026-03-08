`MARU_ENABLE_INTERNAL_CHECKS` has a few potentially surprising effects.

## X11

`MARU_ENABLE_INTERNAL_CHECKS` can enable process-global X11 error handling behavior.
If you run multiple concurrent contexts and you hit unexpected interactions, this is
one of the first places to investigate.

## Wayland (libdecor path)

When both of these are true:

- `MARU_ENABLE_INTERNAL_CHECKS` is enabled, and
- the Wayland backend is using `libdecor` (CSD mode),

Maru stores `libdecor_get_userdata` in a process-global static function pointer
(`_libdecor_get_userdata_fn`) so that libdecor's global-style error callback can
resolve the owning `MARU_Context` and emit diagnostics.

Notes:

- This global pointer is only compiled in with `MARU_ENABLE_INTERNAL_CHECKS`.
- In normal builds (internal checks off), this path does not use the global pointer.
- The behavior is diagnostic plumbing only; it is not required for core runtime behavior.
