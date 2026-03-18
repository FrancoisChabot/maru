# macOS (Cocoa) Backend Implementation Checklist

This checklist tracks the implementation status of the Maru macOS (Cocoa) backend.

## 1. Core & Context
- [x] Context Creation/Destruction (`maru_createContext_Cocoa`, `maru_destroyContext_Cocoa`)
- [x] Event Pumping (`maru_pumpEvents_Cocoa` with `NSApp nextEventMatchingMask`)
- [x] Context Waking (`maru_wakeContext_Cocoa` via `NSEventTypeApplicationDefined`)
- [x] Context Attribute Updates (`maru_updateContext_Cocoa`)
- [x] Native Handle Access (`_maru_getContextNativeHandle_Cocoa`)
- [x] System Idle Inhibition (`MARU_CONTEXT_ATTR_INHIBIT_IDLE` via `IOPMAssertionCreateWithDescription`)
- [x] High-DPI Awareness (Retina support via `backingScaleFactor`)
- [x] Main Thread Enforcement (Context creation/pump must be on main thread)

## 2. Window Management
- [x] Window Creation/Destruction (`maru_createWindow_Cocoa`, `maru_destroyWindow_Cocoa`)
- [x] Geometry Reporting (`maru_getWindowGeometry_Cocoa`)
- [x] Title Management (`MARU_WINDOW_ATTR_TITLE`)
- [x] Visibility Control (`MARU_WINDOW_ATTR_VISIBLE`)
- [x] Sizing and Positioning (`MARU_WINDOW_ATTR_DIP_SIZE`, `MARU_WINDOW_ATTR_DIP_POSITION`)
- [x] Presentation States (`MARU_WINDOW_ATTR_PRESENTATION_STATE`: Fullscreen, Maximized, Minimized, Normal)
- [x] Focus Requests (`maru_requestWindowFocus_Cocoa`)
- [x] Frame Request (`maru_requestWindowFrame_Cocoa` via `CVDisplayLink`)
- [x] Attention Requests (`maru_requestWindowAttention_Cocoa` via `NSInformationalRequest`)
- [x] Window Icon Support (Sets application icon on macOS)
- [x] Track and report visibility changes (`MARU_WINDOW_STATE_CHANGED_VISIBLE`)
- [x] Track and report presentation state changes (`MARU_WINDOW_STATE_CHANGED_PRESENTATION_STATE`)

## 3. Input & Events
- [x] Keyboard Events (`MARU_EVENT_KEY_CHANGED` / `NSEventTypeKeyDown`, `NSEventTypeKeyUp`)
- [x] Key Translation Table (macOS scancodes to `MARU_Key`)
- [x] Mouse Button Events (`NSEventTypeLeftMouseDown`, etc.)
- [x] Mouse Motion Events (`NSEventTypeMouseMoved` / `NSEventType*MouseDragged`)
- [x] Scroll Wheel Events (`NSEventTypeScrollWheel`)
- [x] Window Resized Events (`NSWindowDidResizeNotification` -> `MARU_EVENT_WINDOW_RESIZED`)
- [x] Close Request Events (`windowShouldClose:` -> `MARU_EVENT_CLOSE_REQUESTED`)
- [x] Window Ready Events (Handled via `pending_ready_event` in `_maru_drain_queued_events`)
- [x] Window Focus Events (`NSWindowDidBecomeKeyNotification`, `NSWindowDidResignKeyNotification`)
- [x] IME / Text Input Support (`NSTextInputClient` implementation)
- [x] Semantic Text Navigation Events (`MARU_EVENT_TEXT_EDIT_NAVIGATION`)
- [ ] **TODO:** Integrate Surrounding Text with `NSTextInputClient` (`MARU_WINDOW_ATTR_SURROUNDING_TEXT`)

## 4. Vulkan Integration
- [x] Extension Reporting (`VK_KHR_surface`, `VK_EXT_metal_surface`)
- [x] Surface Creation (`maru_createVkSurface_Cocoa` via `vkCreateMetalSurfaceEXT`)

## 5. Monitors
- [x] Monitor Enumeration (`[NSScreen screens]`)
- [x] Video Mode Enumeration (`CGDisplayCopyAllDisplayModes`)
- [x] Setting Video Modes (`CGDisplaySetDisplayMode`)
- [x] Monitor Change Notifications (`CGDisplayRegisterReconfigurationCallback`)
- [ ] **TODO:** Detect property changes (scale, resolution) for existing monitors
- [x] Dispatch `MARU_EVENT_MONITOR_MODE_CHANGED` after mode changes

## 6. Cursors & Images
- [x] Standard Cursor Shapes (`NSCursor`)
- [x] Custom Cursor Creation (`initWithImage:hotSpot:`)
- [x] Animated Cursor Support (`_maru_register_animated_cursor`)
- [x] Image Resource Management (`maru_createImage_Cocoa`)
- [x] Support missing standard shapes: `HELP`, `WAIT`, `NESW_RESIZE`, `NWSE_RESIZE` (Fails on macOS)

## 7. Controllers / Gamepads
- [x] Controller Enumeration (`GCController`)
- [x] Haptic Feedback Support (`CHHapticEngine`)
- [x] Standardized Channel Mapping (`GCExtendedGamepad`)
- [x] Controller Change Notifications (`GCControllerDidConnectNotification` / `GCControllerDidDisconnectNotification`)
- [x] Button and Analog Polling

## 8. Data Exchange / Clipboard / DnD
- [x] Clipboard Access (`NSPasteboard` support)
- [x] Drag and Drop Implementation (`NSDraggingDestination`, `NSDraggingSource`)
- [x] Implement `MARU_EVENT_DATA_RELEASED`
- [ ] **TODO:** Implement `MARU_EVENT_DRAG_FINISHED`
- [ ] **TODO:** Support `MARU_DATA_PROVIDE_FLAG_ZERO_COPY` (currently always copies)
