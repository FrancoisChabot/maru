# Maru Completion Matrix

This document tracks the implementation status of the Maru API across different backends.

**Status Key:**
- `❌` : Not started
- `🏗️` : In progress / Partially implemented
- `✅` : Fully implemented and verified
- `N/A`: Not applicable for this backend

## 1. Core & Versioning

| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_getVersion()`                           |   ✅    |   ✅    |   ✅    |   ✅    |

## 2. Context Management

### Functions
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_createContext()`                        |   🏗️    |   🏗️    |   ❌    |   ❌    |
| `maru_destroyContext()`                       |   🏗️    |   🏗️    |   ❌    |   ❌    |
| `maru_updateContext()`                        |   🏗️    |   🏗️    |   🏗️    |   🏗️    |
| `maru_resetContextMetrics()`                  |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_wakeContext()`                          |   ❌    |   ❌    |   ❌    |   ❌    |

### Create Info Fields (`MARU_ContextCreateInfo`)
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `.allocator`                                  |   ✅    |   ✅    |   ✅    |   ✅    |
| `.userdata`                                   |   ✅    |   ✅    |   ✅    |   ✅    |
| `.backend`                                    |   ✅    |   ✅    |   ✅    |   ✅    |
| `.attributes`                                 |   🏗️    |   ❌    |   ❌    |   ❌    |
| `.tuning`                                     |   ❌    |   ❌    |   ❌    |   ❌    |

### Attribute Fields (`MARU_ContextAttributes`)
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `MARU_CONTEXT_ATTR_INHIBITS_SYSTEM_IDLE`      |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_CONTEXT_ATTR_DIAGNOSTICS`               |   ✅    |   ✅    |   ✅    |   ✅    |
| `MARU_CONTEXT_ATTR_EVENT_MASK`                |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_CONTEXT_ATTR_EVENT_CALLBACK`            |   ❌    |   ❌    |   ❌    |   ❌    |

### Passive Accessors & Metrics
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_getContextUserdata()`                   |   ✅    |   ✅    |   ✅    |   ✅    |
| `maru_setContextUserdata()`                   |   ✅    |   ✅    |   ✅    |   ✅    |
| `maru_isContextLost()`                        |   ✅    |   ✅    |   ✅    |   ✅    |
| `maru_isContextReady()`                       |   ✅    |   ✅    |   ✅    |   ✅    |
| `maru_getContextMetrics()`                    |   ❌    |   ❌    |   ❌    |   ❌    |

## 3. Window Management

### Functions
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_createWindow()`                         |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_destroyWindow()`                        |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_updateWindow()`                         |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getWindowGeometry()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_requestWindowFocus()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getWindowBackendHandle()`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_resetWindowMetrics()`                   |   ❌    |   ❌    |   ❌    |   ❌    |

### Create Info Fields (`MARU_WindowCreateInfo`)
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `.attributes`                                 |   ❌    |   ❌    |   ❌    |   ❌    |
| `.app_id`                                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `.content_type`                               |   ❌    |   ❌    |   ❌    |   ❌    |
| `.transparent`                                |   ❌    |   ❌    |   ❌    |   ❌    |
| `.userdata`                                   |   ❌    |   ❌    |   ❌    |   ❌    |

### Attribute Fields (`MARU_WindowAttributes`)
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `MARU_WINDOW_ATTR_TITLE`                      |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_LOGICAL_SIZE`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_FULLSCREEN`                 |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_CURSOR_MODE`                |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_CURSOR`                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_MONITOR`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_MAXIMIZED`                  |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_MIN_SIZE`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_MAX_SIZE`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_POSITION`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_ASPECT_RATIO`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_RESIZABLE`                  |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_DECORATED`                  |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_MOUSE_PASSTHROUGH`          |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_ACCEPT_DROP`                |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_TEXT_INPUT_TYPE`            |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_TEXT_INPUT_RECT`            |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_PRIMARY_SELECTION`          |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_ATTR_EVENT_MASK`                 |   ❌    |   ❌    |   ❌    |   ❌    |

### Passive Accessors & Metrics
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_getWindowUserdata()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowUserdata()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getWindowContext()`                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_isWindowLost()`                         |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_isWindowReady()`                        |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_isWindowFocused()`                      |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_isWindowMaximized()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_isWindowFullscreen()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getWindowEventMask()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getWindowMetrics()`                     |   ❌    |   ❌    |   ❌    |   ❌    |

## 4. Monitors

| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_getMonitors()`                          |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_retainMonitor()`                        |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_releaseMonitor()`                       |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorModes()`                      |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setMonitorMode()`                       |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_resetMonitorMetrics()`                  |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorUserdata()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setMonitorUserdata()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorContext()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_isMonitorLost()`                        |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorMetrics()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorName()`                       |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorPhysicalSize()`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorCurrentMode()`                |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorLogicalPosition()`            |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorLogicalSize()`                |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_isMonitorPrimary()`                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getMonitorScale()`                      |   ❌    |   ❌    |   ❌    |   ❌    |

## 5. Inputs

| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_getKeyboardButtonCount()`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getKeyboardButtonStates()`              |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_isKeyPressed()`                         |   ❌    |   ❌    |   ❌    |   ❌    |

## 6. Cursors

| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_getStandardCursor()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_createCursor()`                         |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_destroyCursor()`                        |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_resetCursorMetrics()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getCursorUserdata()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setCursorUserdata()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_isCursorSystem()`                       |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getCursorMetrics()`                     |   ❌    |   ❌    |   ❌    |   ❌    |

## 7. Events & Dispatch

### Functions
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_pumpEvents()`                           |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_postEvent()`                            |   ❌    |   ❌    |   ❌    |   ❌    |

### Event Types (`MARU_EventMask`)
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `MARU_CLOSE_REQUESTED`                        |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_RESIZED`                         |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_KEY_STATE_CHANGED`                      |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_READY`                           |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_MOUSE_MOVED`                            |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_MOUSE_BUTTON_STATE_CHANGED`             |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_MOUSE_SCROLLED`                         |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_IDLE_STATE_CHANGED`                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_MONITOR_CONNECTION_CHANGED`             |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_MONITOR_MODE_CHANGED`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_SYNC_POINT_REACHED`                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_TEXT_INPUT_RECEIVED`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_FOCUS_CHANGED`                          |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_WINDOW_MAXIMIZED`                       |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_DROP_ENTERED`                           |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_DROP_HOVERED`                           |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_DROP_EXITED`                            |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_DROP_DROPPED`                           |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_TEXT_COMPOSITION_UPDATED`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_DATA_RECEIVED`                          |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_DATA_REQUESTED`                         |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_DATA_CONSUMED`                          |   ❌    |   ❌    |   ❌    |   ❌    |
| `MARU_DRAG_FINISHED`                          |   ❌    |   ❌    |   ❌    |   ❌    |

## 8. Data Exchange (Clipboard/DND)

| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_announceData()`                         |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_provideData()`                          |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_requestData()`                          |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getAvailableMIMETypes()`                |   ❌    |   ❌    |   ❌    |   ❌    |

## 9. Tuning

| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `.wayland.decoration_mode`                    |   N/A   |   ❌    |   N/A   |   N/A   |
| `.wayland.dnd_post_drop_timeout_ms`           |   N/A   |   ❌    |   N/A   |   N/A   |
| `.wayland.enforce_client_side_constraints`    |   N/A   |   ❌    |   N/A   |   N/A   |
| `.x11.idle_poll_interval_ms`                  |   ❌    |   N/A   |   N/A   |   N/A   |
| `.vk_loader`                                  |   ❌    |   ❌    |   ❌    |   ❌    |

## 10. Extensions

### Controllers
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_getControllers()`                       |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_retainController()`                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_releaseController()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_resetControllerMetrics()`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getControllerInfo()`                    |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setControllerHapticLevels()`            |   ❌    |   ❌    |   ❌    |   ❌    |

### Vulkan
| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_getVkExtensions()`                      |   🏗️    |   🏗️    |   ❌    |   ❌    |
| `maru_createVkSurface()`                      |   🏗️    |   🏗️    |   ❌    |   ❌    |

## 11. Convenience Functions

| Feature                                       |   X11   | Wayland | Windows |  Cocoa  |
| :-------------------------------------------- | :-----: | :-----: | :-----: | :-----: |
| `maru_setContextInhibitsSystemIdle()`         |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowTitle()`                       |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowSize()`                        |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowFullscreen()`                  |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowMaximized()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowCursorMode()`                  |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowCursor()`                      |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowMonitor()`                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowMinSize()`                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowMaxSize()`                     |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowAspectRatio()`                 |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowResizable()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowDecorated()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowMousePassthrough()`            |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowTextInputType()`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowTextInputRect()`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowAcceptDrop()`                  |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_setWindowEventMask()`                   |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_requestText()`                          |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getContextEventMetrics()`               |   ❌    |   ❌    |   ❌    |   ❌    |
| `maru_getDiagnosticString()`                  |   ❌    |   ❌    |   ❌    |   ❌    |
