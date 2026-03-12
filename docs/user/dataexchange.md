# Data Exchange (Clipboard & Drag-and-Drop)

Maru provides a unified API for exchanging data with the operating system and other applications. This covers the system clipboard, the primary selection (on X11/Wayland), and Drag-and-Drop (DnD).

## Concepts

- **Target**: The location of the data (`MARU_DATA_EXCHANGE_TARGET_CLIPBOARD`, `MARU_DATA_EXCHANGE_TARGET_PRIMARY`, or `MARU_DATA_EXCHANGE_TARGET_DRAG_DROP`).
- **MIME Type**: A string identifying the data format (e.g., `text/plain`, `image/png`).
- **Lazy Evaluation**: Maru uses a "lazy" approach for providing data. You announce what formats you have, and only provide the actual bytes when another application requests them.

---

## Clipboard (Copy & Paste)

### Copying to Clipboard

To put data on the clipboard, you first announce the available formats.

```c
const char *mimes[] = {"text/plain"};
MARU_MIMETypeList mime_types = {mimes, 1};
maru_announceData(window, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD, mime_types, MARU_DROP_ACTION_NONE);
```

When another app wants to paste, Maru will fire a `MARU_EVENT_DATA_REQUESTED` event. You must respond with `maru_provideData`.

```c
if (type == MARU_EVENT_DATA_REQUESTED) {
    const char *my_text = "Hello from Maru!";
    maru_provideData(&event->data_requested, my_text, strlen(my_text), MARU_DATA_PROVIDE_FLAG_NONE);
}
```

### Pasting from Clipboard

To get data, you first request it for a specific MIME type.

```c
maru_requestData(window, MARU_DATA_EXCHANGE_TARGET_CLIPBOARD, "text/plain", my_tag);
```

The data will arrive later in a `MARU_EVENT_DATA_RECEIVED` event.

```c
if (type == MARU_EVENT_DATA_RECEIVED) {
    printf("Received: %.*s\n", (int)event->data_received.dip_size, (char*)event->data_received.data);
}
```

---

## Drag-and-Drop (DnD)

### Receiving Drops

To receive drops, you must set the `accept_drop` attribute on your window.

```c
MARU_WindowAttributes attrs = {0};
attrs.accept_drop = true;
maru_updateWindow(window, MARU_WINDOW_ATTR_ACCEPT_DROP, &attrs);
```

You will then receive the following events:

- `MARU_EVENT_DROP_ENTERED`: Fired when a drag enters the window.
- `MARU_EVENT_DROP_HOVERED`: Fired as the drag moves over the window.
- `MARU_EVENT_DROP_EXITED`: Fired when the drag leaves.
- `MARU_EVENT_DROP_DROPPED`: Fired when the user releases the mouse.

In `DROP_ENTERED` and `DROP_HOVERED`, you must provide feedback to the OS about whether you accept the drop and what action (Copy, Move, Link) you will take.

The `session` handle delivered in DnD events is callback-scoped: do not store it after the callback returns. If you need to carry application state across callbacks for the same drag session, use `maru_setDropSessionUserdata()` and recover it from the next callback's `session` handle.

```c
if (type == MARU_EVENT_DROP_HOVERED) {
    // We only accept plain text
    if (has_mime_type(event->drop_hovered.available_types, "text/plain")) {
        maru_setDropSessionAction(event->drop_hovered.session, MARU_DROP_ACTION_COPY);
    } else {
        maru_setDropSessionAction(event->drop_hovered.session, MARU_DROP_ACTION_NONE);
    }
}
```

When `MARU_EVENT_DROP_DROPPED` occurs, you can then call `maru_requestData` with `MARU_DATA_EXCHANGE_TARGET_DRAG_DROP` to get the payload.

### Initiating Drags

To start a drag, call `maru_announceData` with `MARU_DATA_EXCHANGE_TARGET_DRAG_DROP`. The OS will then take over the cursor until the drag is finished or cancelled.

Next: [Vulkan Integration](vulkan.md)
