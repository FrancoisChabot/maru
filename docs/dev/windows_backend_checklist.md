# Windows Backend Implementation Checklist

This checklist tracks the implementation status of the Maru Windows (Win32) backend.

## 1. Core & Context
- [x] Context Creation/Destruction (`maru_createContext_Windows`, `maru_destroyContext_Windows`)
- [x] Event Pumping (`maru_pumpEvents_Windows`)
- [x] Context Waking (`maru_wakeContext_Windows`)
- [x] Context Attribute Updates (`maru_updateContext_Windows`)
- [x] Native Handle Access (`_maru_getContextNativeHandle_Windows`)
- [x] **DONE:** Implement System Idle Inhibition (`MARU_CONTEXT_ATTR_INHIBIT_IDLE` via `SetThreadExecutionState`)
- [x] Unicode Support (`UNICODE` / `_UNICODE` defined)
- [x] High-DPI Awareness (Per-monitor V2)

## 2. Window Management
- [x] Window Creation/Destruction (`maru_createWindow_Windows`, `maru_destroyWindow_Windows`)
- [x] Geometry Reporting (`maru_getWindowGeometry_Windows`)
- [x] Title Management (`MARU_WINDOW_ATTR_TITLE`)
- [x] Visibility Control (`MARU_WINDOW_ATTR_VISIBLE` with white-flash mitigation)
- [x] Sizing and Positioning (`MARU_WINDOW_ATTR_DIP_SIZE`, `MARU_WINDOW_ATTR_DIP_POSITION`)
- [x] Fullscreen States (via `MARU_WINDOW_ATTR_PRESENTATION_STATE`)
- [x] Focus Requests (`maru_requestWindowFocus_Windows`)
- [x] **DONE:** Frame Request (`maru_requestWindowFrame_Windows`)
- [x] Attention Requests (`maru_requestWindowAttention_Windows` via `FlashWindow`)
- [x] Window Icon Support

## 3. Input & Events
- [x] Keyboard Events (`MARU_EVENT_KEY_CHANGED` / `WM_KEYDOWN`, `WM_KEYUP`)
- [x] Key Translation Table (Virtual-Key codes to `MARU_Key`)
- [x] Mouse Button Events (`WM_LBUTTONDOWN`, etc.)
- [x] Mouse Motion Events (`WM_MOUSEMOVE` / `WM_INPUT` for raw)
- [x] Scroll Wheel Events (`WM_MOUSEWHEEL`, `WM_MOUSEHWHEEL`)
- [x] Window Resized Events (`WM_SIZE` / `WM_EXITSIZEMOVE`)
- [x] Close Request Events (`WM_CLOSE` -> `MARU_EVENT_CLOSE_REQUESTED`)
- [x] Window Ready Events (Handled via `pending_ready_event` in `_maru_drain_queued_events`)
- [x] Window Focus Events (`WM_SETFOCUS`, `WM_KILLFOCUS`)
- [ ] **TODO:** IME / Text Input Support

## 4. Vulkan Integration
- [x] Extension Reporting (`VK_KHR_surface`, `VK_KHR_win32_surface`)
- [x] Surface Creation (`maru_createVkSurface_Windows` via `vkCreateWin32SurfaceKHR`)

## 5. Monitors
- [x] **DONE:** Monitor Enumeration (`EnumDisplayMonitors`)
- [x] **DONE:** Video Mode Enumeration (`EnumDisplaySettings`)
- [x] **DONE:** Setting Video Modes (`ChangeDisplaySettingsEx`)
- [x] **DONE:** Monitor Change Notifications (`WM_DISPLAYCHANGE`)

## 6. Cursors & Images
- [x] Standard Cursor Shapes (`LoadCursor`)
- [x] Custom Cursor Creation (`CreateIconIndirect`)
- [x] Image Resource Management (`maru_createImage_Windows`)

## 7. Controllers / Gamepads (STUBBED)
- [ ] **TODO:** Controller Enumeration (XInput / Raw Input / WinRT Gamepad)
- [ ] **TODO:** Haptic Feedback Support

## 8. Data Exchange / Clipboard / DnD (STUBBED)
- [x] **DONE:** Clipboard Access (`OpenClipboard`, `GetClipboardData`, etc.)
- [ ] **TODO:** Drag and Drop Implementation (`IDropTarget`, `IDropSource`)
