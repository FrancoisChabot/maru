# MARU

A straightforward-but-complete modern light Platform Abstraction Layer.

Why MARU?

- It has no dynamic global state whatsoever.
- It's laser-focused on performance and footprint
- API use is aggressively validated in debug, and all guardrails can be disabled for maximum performance.
- It has an opt-in recoverable API for CFFI integrations. 

## The MARU Event Model: Scoped Direct Dispatch

Unlike libraries that use persistent global callbacks (GLFW) or internal event queueing (SDL), MARU uses **Scoped Direct Dispatch**. 

You provide your event handler directly to `maru_pumpEvents()`. This provides three major benefits:

1.  **Temporal Predictability:** Your code is only ever called within the stack frame of `maru_pumpEvents`. MARU guarantees no unexpected re-entrancy during other API calls like window updates or destruction.
2.  **Contextual Flexibility:** You can easily swap handlers for different application states (e.g., `handle_menu_events` vs `handle_game_events`) without managing persistent library state.
3.  **Zero-Overhead:** Events are dispatched immediately from the OS loop to your function. MARU does not maintain an internal heap-allocated event queue, facilitating integration into engines that already implement their own queueing.

*Note: Any pointers provided within event payloads (e.g., strings in text input events) are only valid for the duration of the callback.*
