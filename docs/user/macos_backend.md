# macOS Backend

This page covers specific considerations for the macOS (Cocoa) backend.

*(This backend is currently under development. Detailed documentation will be added as features are finalized.)*

## Overview

The macOS backend uses the Cocoa framework to create and manage windows. It supports:

- Full Retina/High-DPI support via `NSWindow`.
- Native macOS event loop integration.
- Vulkan rendering via the `VK_EXT_metal_surface` extension (through MoltenVK).

## Configuration

You can tune backend-specific behavior via `MARU_ContextCreateInfo`:

```c
MARU_ContextCreateInfo info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
// info.tuning.macos.some_property = value;
```

## Known Limitations

- **Lifecycle**: macOS apps must be bundled (.app) and follow specific lifecycle requirements (e.g., `NSApplication` must run on the main thread).
- **Fullscreen**: Native "Lion-style" fullscreen and traditional "exclusive" modes are being refined.
