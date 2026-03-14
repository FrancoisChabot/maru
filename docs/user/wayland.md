# Wayland Backend Contract

This page defines the user-facing behavior contract for Maru on Wayland.

## General Model

- Maru follows Wayland compositor authority rules.
- Requests that conflict with compositor control fail with `MARU_FAILURE` and
  emit a diagnostic (`MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED`).
- Many advanced behaviors are protocol-dependent and may vary by compositor.

## Guaranteed Behavior

- Window creation, update loop, event pumping, and native Wayland handle access.
- Vulkan integration (`VK_KHR_wayland_surface`) when available.
- Keyboard/mouse input, including XKB-backed key interpretation.
- Clipboard / drag-and-drop / primary selection when matching protocols exist.
- IME text-input integration through `zwp_text_input_manager_v3` when available.
- Cursor creation and runtime cursor updates.
- Monitor enumeration and monitor mode listing.

## Backend-Specific Limitations

### Window Position

- Setting `MARU_WINDOW_ATTR_DIP_POSITION` is unsupported on Wayland.
- Expected result: `MARU_FAILURE` + diagnostic.

### Monitor Targeting

- Setting a monitor target is only meaningful in fullscreen mode.
- Non-fullscreen monitor targeting returns `MARU_FAILURE` + diagnostic.

### Window Icon

- Setting `MARU_WINDOW_ATTR_ICON` is shell/compositor-managed on Wayland.
- Maru stores the request as intent, but icon application is not guaranteed.

### Monitor Mode Changes

- `maru_setMonitorMode` is unsupported on Wayland and returns `MARU_FAILURE`.

### Focus/Attention Requests

- `maru_requestWindowFocus` and attention requests depend on
  `xdg_activation_v1`.
- If unavailable, requests return `MARU_FAILURE` + diagnostic.

### Minimize / Restore

- Minimize/restore behavior is compositor and shell-path dependent.
- If no `xdg_toplevel` is available for the active decoration path, minimize
  requests return `MARU_FAILURE` + diagnostic.
- Restore transitions may also rely on focus activation support (`xdg_activation_v1`)
  and can degrade when unavailable.

### Clipboard Requirements

- Wayland's `wl_data_device` protocol requires a valid surface for data exchange.
- Maru requires at least one active window in the context to perform clipboard operations.
- If no window exists, clipboard requests return `MARU_FAILURE` + diagnostic (`MARU_DIAGNOSTIC_FEATURE_UNSUPPORTED`).

## Optional Protocol Features

If the compositor does not expose these globals, related requests may fail or
degrade gracefully with diagnostics:

- `zwp_text_input_manager_v3` for advanced IME flows.
- `wl_data_device_manager` for clipboard and drag-and-drop.
- `zwp_pointer_constraints_v1` and `zwp_relative_pointer_manager_v1` for pointer lock/raw motion.
- `wp_viewporter` for viewport destination sizing.
- `zwp_idle_inhibit_manager_v1` for idle inhibition.
- `xdg_activation_v1` for focus activation flows.

## Practical Guidance

- Treat Wayland as capability-driven, not assumption-driven.
- Check call return codes and subscribe to diagnostics.
- For cross-desktop reliability, assume protocol variance across compositors.
