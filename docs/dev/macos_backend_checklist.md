# macOS (Cocoa) Backend Implementation Checklist

This document tracks the detailed implementation status of the Maru macOS backend.

## 1. Core & Context
- [x] **Context Creation/Destruction** (`maru_createContext_Cocoa`, `maru_destroyContext_Cocoa`)
- [x] **Event Pumping** (`maru_pumpEvents_Cocoa`)
- [x] **Context Waking** (`maru_wakeContext_Cocoa`)
- [x] **Context Attribute Updates** (`maru_updateContext_Cocoa`)
- [x] **Native Handle Access** (`_maru_getContextNativeHandle_Cocoa`)
- [ ] **System Idle Inhibition** (`MARU_CONTEXT_ATTR_INHIBIT_IDLE`)
  - [ ] Implement using `IOPMAssertionCreateWithDescription`.
- [ ] **Activation Policy Support**
  - [ ] Support `NSApplicationActivationPolicyProhibited` or `Accessory` via `create_info`. Currently hardcoded to `Regular`.
- [x] **Main Thread Enforcement**
  - [x] Verify context creation and pump happen on the main thread.

## 2. Window Management
- [x] **Window Creation/Destruction** (`maru_createWindow_Cocoa`, `maru_destroyWindow_Cocoa`)
- [x] **Geometry Reporting** (`maru_getWindowGeometry_Cocoa`)
- [x] **Title Management** (`MARU_WINDOW_ATTR_TITLE`)
- [x] **Sizing and Positioning** (`MARU_WINDOW_ATTR_DIP_SIZE`, `MARU_WINDOW_ATTR_DIP_POSITION`)
- [x] **Visibility Control** (`MARU_WINDOW_ATTR_VISIBLE`)
- [x] **Min/Max/Fullscreen States** (`MARU_WINDOW_ATTR_MINIMIZED`, `MARU_WINDOW_ATTR_MAXIMIZED`, `MARU_WINDOW_ATTR_FULLSCREEN`)
- [x] **Focus Requests** (`maru_requestWindowFocus_Cocoa`)
- [x] **Attention Requests** (`maru_requestWindowAttention_Cocoa`)
- [ ] **Frame Request Synchronization** (`maru_requestWindowFrame_Cocoa`)
  - [ ] Currently posts event immediately. Needs `CVDisplayLink` or `dispatch_source_t` (type `VBL`) synchronization for proper frame timing.
- [ ] **Window Attribute Constraints**
  - [ ] Implement Aspect Ratio constraints (`MARU_WINDOW_ATTR_ASPECT_RATIO`).
  - [ ] Implement Min/Max size constraints (`MARU_WINDOW_ATTR_DIP_MIN_SIZE`, `MARU_WINDOW_ATTR_DIP_MAX_SIZE`).
- [ ] **Content Metadata**
  - [ ] Support `app_id` and `content_type` during creation.

## 3. Input & Events
- [x] **Keyboard Events** (`MARU_EVENT_KEY_CHANGED`)
- [ ] **Key Translation Table**
  - [ ] Validate and complete mapping for all `MARU_Key` values (currently covers basic US layout).
- [x] **Modifier Mapping** (`Shift`, `Control`, `Option`, `Command`, `CapsLock`, `NumLock`)
- [x] **Mouse Button Events** (`MARU_EVENT_MOUSE_BUTTON_CHANGED`)
- [x] **Mouse Motion Events** (`MARU_EVENT_MOUSE_MOVED`)
- [x] **Scroll Wheel Events** (`MARU_EVENT_MOUSE_SCROLLED`)
- [x] **Window State Events** (`RESIZED`, `CLOSE_REQUESTED`, `READY`, `FOCUSED`)
- [x] **IME / Text Input Support**
  - [x] Implement `NSTextInputClient` protocol in `MARU_ContentView`.
  - [x] Emit `MARU_EVENT_TEXT_EDIT_STARTED`, `UPDATED`, `COMMITTED`, `ENDED`.
  - [x] Support `MARU_WINDOW_ATTR_TEXT_INPUT_TYPE` and `MARU_WINDOW_ATTR_DIP_TEXT_INPUT_RECT`.
  - [x] Support surrounding text attributes.

## 4. Vulkan Integration
- [x] **Extension Reporting** (`VK_KHR_surface`, `VK_EXT_metal_surface`)
- [x] **Surface Creation** (`maru_createVkSurface_Cocoa` via `CAMetalLayer`)
- [ ] **Surface Loss/Maintenance**
  - [ ] Ensure surface handles window resizing and display changes correctly.

## 5. Monitors
- [x] **Monitor Enumeration** (`maru_getMonitors_Cocoa`)
- [x] **Video Mode Enumeration** (`maru_getMonitorModes_Cocoa`)
- [ ] **Monitor Change Notifications** (`MARU_EVENT_MONITOR_CHANGED`)
  - [ ] Use `CGDisplayRegisterReconfigurationCallback`.
- [ ] **Monitor Cache Invalidation**
  - [ ] Refresh monitor list when hardware changes occur.
- [ ] **Video Mode Switching** (`maru_setMonitorMode_Cocoa`)
  - [ ] Currently stubbed (Returns `MARU_FAILURE`). Decide if this should remain stubbed or use `CGDisplaySetDisplayMode`.

## 6. Cursors & Images
- [x] **Standard Cursor Shapes** (`maru_getStandardCursor_Cocoa`)
- [x] **Custom Cursor Creation** (`maru_createCursor_Cocoa`)
- [x] **Animated Cursor Support**
- [x] **Image Resource Management** (`maru_createImage_Cocoa`)

## 7. Controllers / Gamepads
- [x] **Controller Enumeration** (`GCController` integration)
- [x] **Standard Axis/Button Mapping** (Basic support for Extended Gamepads)
- [ ] **Enhanced Mapping**
  - [ ] Support for non-extended gamepads.
  - [ ] Support for more than standard buttons/analogs if available.
- [ ] **Haptic Feedback Support** (`maru_setControllerHapticLevels_Cocoa`)
  - [ ] Implement using `CHHapticEngine` or `GCDeviceHaptics`.

## 8. Data Exchange (Clipboard & DnD)
- [x] **Clipboard Access** (`NSPasteboard`)
  - [x] Implement `maru_announceClipboardData`.
  - [x] Implement `maru_requestClipboardData`.
  - [x] Implement `maru_getAvailableClipboardMIMETypes`.
- [ ] **Drag and Drop Implementation** (STUBBED)
  - [ ] Implement `NSDraggingDestination` and `NSDraggingSource` protocols.
  - [ ] Emit `MARU_EVENT_DROP_ENTERED`, `HOVERED`, `EXITED`, `DROPPED`.
  - [ ] Support `maru_announceDragData` and `maru_requestDropData`.
