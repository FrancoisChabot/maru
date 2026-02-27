# TODO

## Cross-Backend

- [ ] Emit `MARU_EVENT_DATA_CONSUMED` for outbound drag-and-drop on Wayland.
  - Current behavior: Wayland emits `MARU_EVENT_DRAG_FINISHED` but does not emit `MARU_EVENT_DATA_CONSUMED`.
  - Expected behavior: when the compositor/target reports final action and payload consumption, dispatch `MARU_EVENT_DATA_CONSUMED` (with target/action/mime/data metadata as available).

- [ ] Emit `MARU_EVENT_DATA_CONSUMED` for outbound drag-and-drop on X11.
  - Current behavior: X11 outbound XDND now emits `MARU_EVENT_DRAG_FINISHED`, but not `MARU_EVENT_DATA_CONSUMED`.
  - Expected behavior: dispatch consumed event when drop completion/action is known.

## X11 Backend

### Data Exchange / DnD

- [ ] Implement robust INCR transfers (clipboard + DnD) for large payloads.
- [ ] Implement external-owner MIME discovery via `TARGETS` requests for clipboard/primary.
- [ ] Improve outbound XDND action negotiation (copy/move/link), not just basic copy-first fallback.
- [ ] Add outbound XDND completion watchdog/fallback when `XdndFinished` is never received.
- [ ] Validate outbound XDND interop across common targets (GTK, Qt, Chromium, file managers).
- [ ] Validate inbound XDND modifiers/action mapping and tighten parity with Wayland semantics.

### Windowing / Context

- [ ] Implement `MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE` on X11.
- [ ] Do a full create-vs-update parity pass for window attributes (single-source apply path to prevent drift).

### Text / IME

**N.B.**: Don't implement this until we have someone ask for it.

- [ ] Improve surrounding-text integration parity (currently basic storage/plumbing only).
- [ ] Validate text-input behavior against more IME/toolkit combinations on X11.

### Validation / Coverage

- [ ] Add automated tests for X11 dataexchange and XDND state machines (inbound + outbound paths).
- [ ] Add regression tests for creation-time attribute application (title/visibility/minimized/fullscreen/maximized/position/hints).
