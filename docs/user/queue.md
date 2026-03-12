# Event Queues

`MARU_Queue` provides an optional, double-buffered snapshot mechanism to consume events. While `maru_pumpEvents()` uses direct callback dispatch, `MARU_Queue` lets you decouple event gathering from event processing and scan events safely from multiple threads.

## The Push-Commit-Scan Lifecycle

A `MARU_Queue` operates in three distinct phases:

1.  **PUSH**: Copy queue-safe callback events into the queue's active buffer.
2.  **COMMIT**: Create a stable snapshot of the active buffer.
3.  **SCAN**: Iterate over the stable snapshot.

### 1. Pushing
On the **primary thread**, call `maru_pumpEvents(...)` and forward queue-safe events into `maru_queue_push(...)` from your callback.

Use `MARU_QUEUE_SAFE_EVENT_MASK` when pumping, or `maru_isQueueSafeEventId(type)` if you prefer to filter inside your callback.
This mask intentionally excludes data-exchange events, monitor/controller handle events, and pointer-bearing text-edit payloads.

### 2. Committing
When ready to process pushed events, call `maru_queue_commit()` on the **primary thread**. This freezes the active buffer as the stable snapshot, then clears the active buffer for the next frame.

### 3. Scanning
Call `maru_queue_scan()` to iterate the stable snapshot. Scanning can happen from **any thread**.

## Threading and Synchronization

- `maru_queue_push()` and `maru_queue_commit()` **MUST** be called from the primary thread.
- `maru_queue_scan()` is safe for concurrent readers and can be called from multiple threads simultaneously.
- **Synchronization**: `MARU_Queue` is NOT a lock-free SPMC queue. You **must** ensure that `maru_queue_scan()` does not run concurrently with `maru_queue_commit()`. A read-write lock (where `commit` is the writer and `scan` is the reader) or a thread barrier is required if scanning from worker threads.

## C API Example

```c
#include <maru/maru.h>

static void queue_capture_cb(MARU_EventId type, MARU_Window *window,
                             const MARU_Event *evt, void *userdata) {
    MARU_Queue *queue = (MARU_Queue *)userdata;
    maru_queue_push(queue, type, window, evt);
}

static void on_queue_event(MARU_EventId type, MARU_Window *window,
                           const MARU_Event *evt, void *userdata) {
    (void)window;
    (void)evt;
    if (type == MARU_EVENT_CLOSE_REQUESTED) {
        *(bool *)userdata = false;
    }
}

int main() {
    MARU_Context *ctx = ...;
    MARU_Queue *queue = NULL;
    maru_queue_create(ctx, 1024, &queue);

    bool running = true;
    while (running) {
        // 1. Pump queue-safe events and push each callback event into queue
        maru_pumpEvents(ctx, 16, MARU_QUEUE_SAFE_EVENT_MASK, queue_capture_cb,
                        queue);

        // 2. Publish snapshot
        maru_queue_commit(queue);

        // 3. Consume snapshot
        maru_queue_scan(queue, MARU_ALL_EVENTS, on_queue_event, &running);
    }

    maru_queue_destroy(queue);
}
```

## Event Coalescence

`MARU_Queue` supports optional event coalescence to prevent high-frequency events (like mouse motion) from overwhelming your application. When enabled for a specific event type, if the latest event in the queue matches the type and window of a new incoming event, they are combined.

Coalescence is opt-in per event type via a bitmask.

### Supported Coalescence Logic
- **`MARU_EVENT_MOUSE_MOVED`**: Updates the absolute dip_position to the latest, and accumulates both `delta` and `raw_delta`.
- **`MARU_EVENT_MOUSE_SCROLLED`**: Accumulates both `delta` and `steps`.
- **`MARU_EVENT_WINDOW_RESIZED`**: Updates the geometry to the latest.
- **Other events**: If opted-in, the existing event is overwritten by the latest one.

### Example (C)
```c
maru_queue_set_coalesce_mask(queue,
    MARU_MASK_MOUSE_MOVED | MARU_MASK_MOUSE_SCROLLED);
```

### Example (C++)
```cpp
queue.setCoalesceMask(MARU_MASK_MOUSE_MOVED | MARU_MASK_MOUSE_SCROLLED);
```

## Overflow Compaction

When the active queue reaches capacity, `MARU_Queue` attempts a compaction sweep before dropping incoming events:

- For coalescible types, only the latest event per `(type, window)` is kept.
- Older duplicates are folded into the survivor using the same coalescing semantics (`delta`/`raw_delta`/`steps` accumulation where applicable).
- Non-coalescible events preserve relative order.
- If no space is freed, the new event is dropped.

You can inspect cumulative queue metrics (including peak occupancy and per-event drop counters):

```c
MARU_QueueMetrics metrics = {0};
maru_queue_get_metrics(queue, &metrics);

// Reset between sampling windows if desired
maru_queue_reset_metrics(queue);
```

```cpp
MARU_QueueMetrics metrics = queue.metrics();
queue.resetMetrics();
```

## C++ API Example (RAII)

```cpp
#include <maru/maru.hpp>

static void queue_capture_cb(MARU_EventId type, MARU_Window *window,
                             const MARU_Event *evt, void *userdata) {
    auto *queue = static_cast<maru::Queue *>(userdata);
    queue->push(type, window, *evt);
}

int main() {
    auto ctx_result = maru::Context::create();
    maru::Context &ctx = *ctx_result;
    auto queue_result = maru::Queue::create(ctx, 1024);
    maru::Queue &queue = *queue_result;

    bool running = true;
    while (running) {
        ctx.pumpEvents(16, MARU_QUEUE_SAFE_EVENT_MASK, queue_capture_cb,
                       &queue);
        queue.commit();

        queue.scan(maru::overloads{
            [&](maru::WindowCloseEvent) { running = false; },
            [&](maru::MouseMotionEvent e) { /* ... */ }
        });
    }
}
```

## Performance Considerations

- **Memory Layout**: `MARU_Queue` uses a bulk-allocated, 64-byte aligned memory layout to minimize cache misses and false sharing during scanning.
- **Lock-Free**: The active/stable buffer swap in `commit()` is O(1) and designed for high throughput on the main thread.
- **Filtering**: Use `MARU_QUEUE_SAFE_EVENT_MASK` at pump-time and a scan mask in `maru_queue_scan()` to minimize queue traffic and scan work.
