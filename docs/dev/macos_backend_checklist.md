# macOS Backend Implementation Checklist

This checklist tracks the implementation status of the Maru macOS (Cocoa) backend.

## 1. Core & Context
- [x] Context Creation/Destruction (`maru_createContext_Cocoa`, `maru_destroyContext_Cocoa`)
- [x] Event Pumping (`maru_pumpEvents_Cocoa`)
- [x] Context Waking (`maru_wakeContext_Cocoa`)
- [x] Context Attribute Updates (`maru_updateContext_Cocoa`)
- [x] Native Handle Access (`_maru_getContextNativeHandle_Cocoa`)
- [ ] **TODO:** Implement System Idle Inhibition (`MARU_CONTEXT_ATTR_INHIBIT_IDLE`)
- [ ] **TODO:** Support for `NSApplicationActivationPolicyProhibited` or `Accessory` if requested.

## 2. Window Management
- [x] Window Creation/Destruction (`maru_createWindow_Cocoa`, `maru_destroyWindow_Cocoa`)
- [x] Geometry Reporting (`maru_getWindowGeometry_Cocoa`)
- [x] Title Management (`MARU_WINDOW_ATTR_TITLE`)
- [x] Sizing and Positioning (`MARU_WINDOW_ATTR_LOGICAL_SIZE`, `MARU_WINDOW_ATTR_POSITION`)
- [x] Visibility Control (`MARU_WINDOW_ATTR_VISIBLE`)
- [x] Min/Max/Fullscreen States (`MARU_WINDOW_ATTR_MINIMIZED`, `MARU_WINDOW_ATTR_MAXIMIZED`, `MARU_WINDOW_ATTR_FULLSCREEN`)
- [x] Focus Requests (`maru_requestWindowFocus_Cocoa`)
- [x] Attention Requests (`maru_requestWindowAttention_Cocoa`)
- [ ] **TODO:** Implement Frame Request (`maru_requestWindowFrame_Cocoa`)
- [x] Window Icon Support

## 3. Input & Events
- [x] Keyboard Events (`MARU_EVENT_KEY_STATE_CHANGED`)
- [x] Key Translation Table (Incomplete, needs validation for all keys)
- [x] Modifier Mapping (`Shift`, `Control`, `Option`, `Command`, `CapsLock`)
- [x] Mouse Button Events (`MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED`)
- [x] Mouse Motion Events (`MARU_EVENT_MOUSE_MOVED`)
- [x] Scroll Wheel Events (`MARU_EVENT_MOUSE_SCROLLED`)
- [x] Window Resized Events (`MARU_EVENT_WINDOW_RESIZED`)
- [x] Close Request Events (`MARU_EVENT_CLOSE_REQUESTED`)
- [x] Window Ready Events (`MARU_EVENT_WINDOW_READY`)
- [x] Mouse Enter/Leave Events (Not defined by MARU API)
- [x] Window Focus Events (Dispatched via Delegate)
- [ ] **TODO:** IME / Text Input Support

## 4. Vulkan Integration
- [x] Extension Reporting (`VK_KHR_surface`, `VK_EXT_metal_surface`)
- [x] Surface Creation (`maru_createVkSurface_Cocoa` via `CAMetalLayer`)

## 5. Monitors (STUBBED)
- [x] Monitor Enumeration (`maru_getMonitors_Cocoa`)
- [x] Video Mode Enumeration (`maru_getMonitorModes_Cocoa`)
- [x] Setting Video Modes (`maru_setMonitorMode_Cocoa`) (Deprecated on macOS, returns MARU_FAILURE)
- [ ] **TODO:** Monitor Change Notifications

## 6. Cursors & Images
- [x] Standard Cursor Shapes (`maru_getStandardCursor_Cocoa`)
- [x] Custom Cursor Creation (`maru_createCursor_Cocoa`)
- [x] Image Resource Management (`maru_createImage_Cocoa`)

## 7. Controllers / Gamepads (STUBBED)
- [ ] **TODO:** Controller Enumeration (GCController integration)
- [ ] **TODO:** Haptic Feedback Support

## 8. Data Exchange / Clipboard / DnD (STUBBED)
- [ ] **TODO:** Clipboard Access (`NSPasteboard`)
- [ ] **TODO:** Drag and Drop Implementation
