# macOS Backend Implementation Checklist

This checklist tracks remaining work for the Maru Cocoa backend after a full
API-surface audit.

## 1. Correctness-Critical Event Payload Fixes

- [x] Copy IME committed text before queueing `MARU_EVENT_TEXT_EDIT_COMMITTED`
- [x] Copy IME preedit text before queueing `MARU_EVENT_TEXT_EDIT_UPDATED`
- [x] Copy clipboard `NSData` payloads before queueing `MARU_EVENT_DATA_RECEIVED`
- [x] Ensure queued `MARU_EVENT_DATA_RELEASED` events do not carry borrowed Objective-C string pointers
- [ ] Audit every `_maru_post_event_internal()` call in Cocoa for borrowed-pointer payloads

## 2. Event Contract Parity

- [x] Emit `MARU_EVENT_WINDOW_STATE_CHANGED` for programmatic `MARU_WINDOW_ATTR_VISIBLE` changes
- [x] Emit `MARU_EVENT_WINDOW_STATE_CHANGED` for programmatic `MARU_WINDOW_ATTR_RESIZABLE` changes
- [x] Emit `MARU_EVENT_WINDOW_STATE_CHANGED` for programmatic `MARU_WINDOW_ATTR_ICON` changes
- [ ] Verify `MARU_EVENT_WINDOW_STATE_CHANGED` field masks match public contract exactly
- [ ] Verify `MARU_EVENT_WINDOW_RESIZED` and `MARU_EVENT_WINDOW_STATE_CHANGED` ordering around fullscreen/minimize transitions
- [ ] Verify cross-context event redirection stays correct for all Cocoa-dispatched window events

## 3. IME / Text Input Semantics

- [x] Stop emitting `MARU_EVENT_TEXT_EDIT_STARTED` just because `text_input_type` changed from `NONE`
- [ ] Ensure one logical text-edit session produces one start and one end
- [x] Remove double-ended session behavior when `text_input_type` changes back to `NONE`
- [ ] Convert Cocoa selection ranges to UTF-8 byte ranges correctly in `MARU_EVENT_TEXT_EDIT_UPDATED`
- [ ] Review `insertText:` replacement/deletion handling against `delete_before_bytes` / `delete_after_bytes`
- [ ] Map `MARU_TEXT_INPUT_TYPE_TEXT`
- [ ] Map `MARU_TEXT_INPUT_TYPE_PASSWORD`
- [ ] Map `MARU_TEXT_INPUT_TYPE_EMAIL`
- [ ] Map `MARU_TEXT_INPUT_TYPE_NUMERIC`
- [ ] Verify `MARU_EVENT_TEXT_EDIT_NAVIGATION` repeat/modifier behavior

## 4. Drag And Drop / Clipboard Semantics

- [ ] Respect `allowed_actions` for outgoing drag sessions
- [ ] Derive incoming `available_actions` from actual Cocoa drag operations instead of hardcoding copy/move/link
- [ ] Populate `MARU_DropEvent.paths` for file drags
- [ ] Verify `MARU_DropEvent.available_types` contents and MIME normalization
- [ ] Set `MARU_DataReleasedEvent.action` correctly for drag/drop releases
- [ ] Audit clipboard and DnD MIME-type caching for lifetime and invalidation correctness
- [ ] Confirm urgent `MARU_EVENT_DATA_REQUESTED` dispatch semantics match other backends

## 5. Mouse / Input Polling Parity

- [x] Initialize default mouse button channels on Cocoa
- [x] Expose the first 5 buttons in `MARU_MouseDefaultButton` order
- [x] Publish per-button `MARU_ChannelInfo` metadata for Cocoa mouse buttons
- [ ] Verify extra mouse buttons beyond the default set are handled or intentionally unsupported
- [ ] Validate scroll-step reporting against public `steps` semantics
- [ ] Validate raw-vs-accelerated mouse delta semantics for `raw_dip_delta`

## 6. Enum Coverage Audit

### Keys

- [ ] Fill missing `MARU_Key` translations where Cocoa exposes native equivalents
- [ ] Explicitly decide behavior for `MARU_KEY_SCROLL_LOCK`
- [ ] Explicitly decide behavior for `MARU_KEY_PRINT_SCREEN`
- [ ] Explicitly decide behavior for `MARU_KEY_PAUSE`
- [ ] Explicitly decide behavior for `MARU_KEY_F21`
- [ ] Explicitly decide behavior for `MARU_KEY_F22`
- [ ] Explicitly decide behavior for `MARU_KEY_F23`
- [ ] Explicitly decide behavior for `MARU_KEY_F24`
- [ ] Explicitly decide behavior for `MARU_KEY_MENU`

### Cursor Shapes And Modes

- [ ] Support or explicitly reject `MARU_CURSOR_SHAPE_HELP`
- [ ] Support or explicitly reject `MARU_CURSOR_SHAPE_WAIT`
- [ ] Support or explicitly reject `MARU_CURSOR_SHAPE_NESW_RESIZE`
- [ ] Support or explicitly reject `MARU_CURSOR_SHAPE_NWSE_RESIZE`
- [ ] Implement real pointer lock for `MARU_CURSOR_LOCKED` or document it as unsupported
- [ ] Revisit whether `MARU_CURSOR_SHAPE_MOVE` should use a closer Cocoa system cursor

### Window And Content Enums

- [ ] Verify all `MARU_WindowPresentationState` transitions preserve the visible/minimized contract
- [ ] Map `MARU_CONTENT_TYPE_NONE`
- [ ] Map `MARU_CONTENT_TYPE_PHOTO`
- [ ] Map `MARU_CONTENT_TYPE_VIDEO`
- [ ] Map `MARU_CONTENT_TYPE_GAME`

## 7. Window API Parity

- [ ] Reconcile per-window icon API with Cocoa's effectively app-global dock icon behavior
- [ ] Decide whether `app_id` should remain `NSWindow.identifier` only or map to a stronger platform concept
- [ ] Verify `MARU_WINDOW_ATTR_MONITOR` semantics on Cocoa for fullscreen targeting
- [ ] Verify `MARU_WINDOW_ATTR_DIP_VIEWPORT_SIZE` semantics against the public geometry contract
- [ ] Verify `MARU_WINDOW_ATTR_DIP_POSITION` coordinate system matches the documented public space
- [ ] Confirm `MARU_WINDOW_ATTR_ASPECT_RATIO` behavior during live resize
- [ ] Confirm window-ready semantics for all native-handle and Vulkan entry points

## 8. Controller Parity

- [ ] Stop advertising full standard channel sets for controllers that do not actually provide them
- [ ] Reconcile `GCMicroGamepad` exposure with Maru standardized controller guarantees
- [ ] Decide how unsupported standard buttons/analogs should be represented on partial controllers
- [ ] Verify standardized-channel flags and ranges match public contracts
- [ ] Verify controller GUID/vendor/product/version data quality on Cocoa
- [ ] Verify disconnect/lost-controller retention semantics against public contract
- [ ] Recheck haptic player lifetime and restart behavior after controller reconnects

## 9. Monitor Parity

- [ ] Verify monitor cache ownership and release behavior on topology changes
- [ ] Deduplicate monitor mode lists returned by CoreGraphics if needed
- [ ] Verify physical-size units from `NSDeviceSize`
- [ ] Verify primary-monitor selection logic matches user-visible expectations
- [ ] Confirm monitor mode change events fire only when observable mode/scale state changed

## 10. Hot Path Allocation Review

- [ ] Remove temporary allocation churn from IME selection-range conversions
- [x] Remove repeated transparent cursor allocation in `resetCursorRects`
- [ ] Check `NSDate` creation in the pump loop for avoidable churn
- [ ] Check drag MIME-type cache rebuilds for repeated per-event heap work
- [ ] Check controller sync path for avoidable realloc/copy churn
- [ ] Re-audit keyboard/mouse dispatch paths to keep them allocation-free

## 11. Redundancy / Cleanup

- [ ] Deduplicate `_maru_cocoa_now_ms()` helpers
- [ ] Reuse shared window-attribute copy helpers where Cocoa currently open-codes copies
- [ ] Consolidate duplicate drop MIME cache rebuild logic
- [ ] Consolidate repeated state-event construction patterns
- [ ] Replace placeholder comments with backend-specific invariants where needed

## 12. Docs And Validation

- [ ] Update `docs/user/macos_backend.md` with current known limitations and guarantees
- [ ] Add targeted tests for queued text/data payload lifetime issues
- [ ] Add tests for Cocoa mouse-button polling metadata
- [ ] Add tests for state-change event emission on programmatic window updates
- [ ] Add tests for IME session lifecycle invariants
- [ ] Add tests for drag/drop action-mask semantics
- [ ] Add tests for unsupported cursor-shape behavior
