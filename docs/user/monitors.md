# Monitors & Displays

Maru provides comprehensive APIs for discovering and managing physical monitors and their display modes.

## Discovering Monitors

You can get a list of all currently connected monitors using `maru_getMonitors`.

```c
MARU_MonitorList monitor_list = {0};
if (maru_getMonitors(context, &monitor_list) != MARU_SUCCESS) {
    // handle failure
}

for (uint32_t i = 0; i < monitor_list.count; ++i) {
    const char *name = maru_getMonitorName(monitor_list.monitors[i]);
    bool is_primary = maru_isMonitorPrimary(monitor_list.monitors[i]);
    printf("Monitor %d: %s %s\n", i, name, is_primary ? "(Primary)" : "");
}
```

### Important: Lifetime of Monitor Handles

Monitor handles returned by `maru_getMonitors` or delivered in events are **transient**. They are only guaranteed to be valid until the next call to `maru_pumpEvents`.

If you need to store a monitor handle for longer, you **must** use `maru_retainMonitor` and `maru_releaseMonitor`.

```c
// To store it
maru_retainMonitor(my_monitor);

// When done
maru_releaseMonitor(my_monitor);
```

The hanfle to a retained monitor will remain valid even if the monitor is disconnected.

---

## Monitor Properties

You can query various properties of a monitor:

- **Name**: `maru_getMonitorName(monitor)`
- **Scale Factor**: `maru_getMonitorScale(monitor)` (Useful for high-DPI handling).
- **Dip Position/Size**: `maru_getMonitorDipPosition(monitor)` and `maru_getMonitorDipSize(monitor)`. These define the monitor's place in the virtual desktop.
- **Physical Size**: `maru_getMonitorPhysicalSize(monitor)` (in millimeters).

---

## Video Modes

To see what resolutions and refresh rates a monitor supports:

```c
MARU_VideoModeList modes = {};
maru_getMonitorModes(monitor, &modes);

for (uint32_t i = 0; i < modes.count; ++i) {
    printf("Mode %d: %dx%d @ %.2fHz\n", i, 
           (int)modes.modes[i].dip_size.x, (int)modes.modes[i].dip_size.y,
           modes.modes[i].refresh_rate_millihz / 1000.0f);
}
```

You can change the current video mode using `maru_setMonitorMode`. This is typically used for "exclusive" fullscreen games.

---

## Handling Monitor Changes

Connect and disconnect events are delivered via the event queue.

```c
void handle_event(MARU_EventId type, MARU_Window *window, const MARU_Event *event, void *userdata) {
    if (type == MARU_EVENT_MONITOR_CONNECTION_CHANGED) {
        if (event->monitor_connection.connected) {
            printf("Monitor connected: %s\n", maru_getMonitorName(event->monitor_connection.monitor));
        } else {
            printf("Monitor disconnected.\n");
        }
    }
}
```

Next: [Data Exchange (Clipboard/DnD)](dataexchange.md) or [Vulkan Integration](vulkan.md)
