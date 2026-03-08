# Event Queues

`MARU_Queue` provides an optional, double-buffered snapshot mechanism to consume events. While `maru_pumpEvents()` uses a direct-dispatch callback model, `MARU_Queue` allows you to decouple event gathering from event processing, making it safe to scan events from multiple threads simultaneously.

## The Pump-Commit-Scan Lifecycle

A `MARU_Queue` operates in three distinct phases:

1.  **PUMP**: Gathering events from the OS.
2.  **COMMIT**: Creating a stable snapshot of gathered events.
3.  **SCAN**: Iterating over the stable snapshot.

### 1. Pumping
On the **primary thread**, you call `maru_queue_pump()`. This functions like `maru_pumpEvents()`, but instead of invoking a user callback immediately, it stores all received events in an internal "active" buffer.

### 2. Committing
Once you are ready to process the gathered events, call `maru_queue_commit()` on the **primary thread**. This "freezes" the current active buffer and makes it the "stable" buffer. The active buffer is then cleared and ready for the next round of pumping.

### 3. Scanning
The stable snapshot can be iterated using `maru_queue_scan()`. This can be done from **any thread**. 

## Threading and Synchronization

- `maru_queue_pump()` and `maru_queue_commit()` **MUST** be called from the primary thread.
- `maru_queue_scan()` is safe for concurrent readers and can be called from multiple threads simultaneously.
- **Synchronization**: `MARU_Queue` is NOT a lock-free SPMC queue. You **must** ensure that `maru_queue_scan()` does not run concurrently with `maru_queue_commit()`. A Read-Write lock (where `commit` is the writer and `scan` is the reader) or a thread barrier is required if scanning from worker threads.

## C API Example

```c
#include <maru/maru.h>

void on_event(MARU_EventId type, MARU_Window *window, const MARU_Event *evt, void *userdata) {
    if (type == MARU_EVENT_CLOSE_REQUESTED) {
        *(bool*)userdata = false;
    }
}

int main() {
    MARU_Context *ctx = ...;
    MARU_Queue *queue;
    maru_queue_create(ctx, 1024, &queue);

    bool running = true;
    while (running) {
        // 1. Gather events
        maru_queue_pump(queue, 16);

        // 2. Make them available for scanning
        maru_queue_commit(queue);

        // 3. Process the stable snapshot
        maru_queue_scan(queue, MARU_ALL_EVENTS, on_event, &running);
    }

    maru_queue_destroy(queue);
}
```

## Event Coalescence

`MARU_Queue` supports optional event coalescence to prevent high-frequency events (like mouse motion) from overwhelming your application. When enabled for a specific event type, if the latest event in the queue matches the type and window of a new incoming event, they are combined.

Coalescence is opt-in per event type via a bitmask.

### Supported Coalescence Logic:
- **`MARU_EVENT_MOUSE_MOVED`**: Updates the absolute position to the latest, and accumulates both `delta` and `raw_delta`.
- **`MARU_EVENT_MOUSE_SCROLLED`**: Accumulates both `delta` and `steps`.
- **`MARU_EVENT_WINDOW_RESIZED`**: Updates the geometry to the latest.
- **Other events**: If opted-in, the existing event is simply overwritten by the latest one.

### Example (C)
```c
// Enable coalescence for mouse motion and scrolling
maru_queue_set_coalesce_mask(queue, MARU_MASK_MOUSE_MOVED | MARU_MASK_MOUSE_SCROLLED);
```

### Example (C++)
```cpp
queue.setCoalesceMask(MARU_MASK_MOUSE_MOVED | MARU_MASK_MOUSE_SCROLLED);
```

## C++ API Example (RAII)

The C++ wrapper `maru::Queue` provides a more idiomatic interface, especially when combined with C++20 visitors.

```cpp
#include <maru/maru.hpp>

int main() {
    maru::Context ctx;
    maru::Queue queue(ctx, 1024);

    bool running = true;
    while (running) {
        queue.pump(16ms);
        queue.commit();

        // Use a visitor to handle events
        queue.scan(maru::overloads{
            [&](maru::WindowCloseEvent) { running = false; },
            [&](maru::MouseMotionEvent e) { /* ... */ }
        });
    }
}
```

## Performance Considerations

- **Memory Layout**: `MARU_Queue` uses a bulk-allocated, 64-byte aligned memory layout to minimize cache misses and false sharing during scanning.
- **Lock-Free**: The internal handover between `pump` and `commit` is designed for high throughput on the main thread.
- **Filtering**: Use `MARU_EventMask` in `maru_queue_scan()` to only iterate over events your application cares about, which can significantly improve performance if the queue contains many high-frequency events (like mouse motion).
