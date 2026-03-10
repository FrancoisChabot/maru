# Windows Backend

This page covers specific considerations for the Windows (Win32) backend.

*(This backend is currently under development. Detailed documentation will be added as features are finalized.)*

## Overview

The Windows backend uses the Win32 API to create and manage windows. It supports:

- Full High-DPI awareness (including per-monitor v2).
- Native Win32 event handling (integrated into the `maru_pumpEvents` loop).
- Vulkan rendering via the `VK_KHR_win32_surface` extension.

## Configuration

You can tune backend-specific behavior via `MARU_ContextCreateInfo`:

```c
MARU_ContextCreateInfo info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
// info.tuning.windows.some_property = value;
```

## Known Limitations

- **Touch and Stylus Input**: Basic mouse emulation is provided; full touch API support is planned.
- **Game Controllers**: Uses XInput for Xbox controllers; Raw Input/DirectInput for others is in progress.
