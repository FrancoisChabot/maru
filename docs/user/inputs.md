# Input Handling

Maru provides two ways to handle input: **Events** (reactive) and **Polling** (state-based).

## Event-Based Input

Events are delivered through the `maru_pumpEvents` callback. This is the preferred way to handle most input, especially for UI elements.

### Keyboard Events

The `MARU_EVENT_KEY_STATE_CHANGED` event is fired when a physical key is pressed or released.

```c
void handle_event(MARU_EventId type, MARU_Window *window, const MARU_Event *event, void *userdata) {
    if (type == MARU_EVENT_KEY_STATE_CHANGED) {
        if (event->key.raw_key == MARU_KEY_ESCAPE && event->key.state == MARU_BUTTON_STATE_PRESSED) {
            // Quit app
        }
    }
}
```

**Note on Text Input:** Do not use keyboard events for text entry. Instead, use the `MARU_EVENT_TEXT_EDIT_COMMIT` event, which handles IME (Input Method Editors), different keyboard layouts, and dead keys correctly.

### Mouse Events

- `MARU_EVENT_MOUSE_MOVED`: Contains current position and delta.
- `MARU_EVENT_MOUSE_BUTTON_STATE_CHANGED`: Fired for clicks.
- `MARU_EVENT_MOUSE_SCROLLED`: Fired for wheel or trackpad scrolling.

```c
if (type == MARU_EVENT_MOUSE_MOVED) {
    float x = event->mouse_motion.position.x;
    float y = event->mouse_motion.position.y;
}
```

---

## State Polling

You can also query the current state of input devices at any time. This is useful for games where you want to check if a key is held down during your update loop.

### Keyboard Polling

```c
if (maru_isKeyPressed(window, MARU_KEY_W)) {
    // Move forward
}
```

### Mouse Polling

```c
if (maru_isMouseButtonPressed(window, MARU_MOUSE_DEFAULT_LEFT)) {
    // Shooting!
}
```

---

## Cursor Management

You can control the visibility and behavior of the mouse cursor via `maru_updateWindow`.

### Cursor Modes

- `MARU_CURSOR_NORMAL`: Standard visible cursor.
- `MARU_CURSOR_HIDDEN`: Cursor is invisible when over the window but behaves normally.
- `MARU_CURSOR_LOCKED`: Cursor is hidden and "grabbed" (confined to the window and providing raw relative motion). Ideal for first-person cameras.

```c
MARU_WindowAttributes attrs = {0};
attrs.cursor_mode = MARU_CURSOR_LOCKED;
maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR_MODE, &attrs);
```

### Custom Cursors

Maru supports hardware-accelerated custom cursors. You create a `MARU_Cursor` from a `MARU_Image` and then apply it to a window.

```c
// Assume 'my_image' is a MARU_Image*
MARU_Cursor *cursor = maru_createCursor(context, my_image, 0, 0); // (0,0) is the hotspot

MARU_WindowAttributes attrs = {0};
attrs.cursor = cursor;
maru_updateWindow(window, MARU_WINDOW_ATTR_CURSOR, &attrs);
```

Next: [Event Queues](queue.md) or [Monitors & Displays](monitors.md)
