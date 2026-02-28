# TODO

## Cross-Backend

- [ ] We need to test zero-copy data provision properly

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
- [x] Do a full create-vs-update parity pass for window attributes (single-source apply path to prevent drift).

### Text / IME

**N.B.**: Don't implement this until we have someone ask for it.

- [ ] Improve surrounding-text integration parity (currently basic storage/plumbing only).
- [ ] Validate text-input behavior against more IME/toolkit combinations on X11.

### Validation / Coverage

- [ ] Determine proper automated testing strategy on each backend. 