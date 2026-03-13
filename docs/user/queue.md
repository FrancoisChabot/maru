# Event Queues

`MARU_Queue` provides an optional, double-buffered snapshot mechanism to consume events. While `maru_pumpEvents()` uses direct callback dispatch, `MARU_Queue` lets you decouple event gathering from event processing and scan events from other threads once you have published a stable snapshot.

The queue API lives in `<maru/queue.h>`. It is a standalone utility surface and is not included by `<maru/maru.h>`.

Because a queue is not attached to any `MARU_Context`, its APIs use plain `bool`
results for queue-local success or failure rather than `MARU_Status`.
`MARU_CONTEXT_LOST` is not meaningful for this utility.

## The Push-Commit-Scan Lifecycle

A `MARU_Queue` operates in three distinct phases:

1.  **PUSH**: Copy queue-safe callback events into the queue's active buffer.
2.  **COMMIT**: Create a stable snapshot of the active buffer.
3.  **SCAN**: Iterate over the stable snapshot.

### 1. Pushing
On the queue creator thread, call `maru_pumpEvents(...)` and forward queue-safe events into `maru_pushQueue(...)` from your callback.

Use `MARU_QUEUE_SAFE_EVENT_MASK` when pumping, or `maru_isQueueSafeEventId(type)` if you prefer to filter inside your callback.
This mask intentionally excludes any event payload that contains transient handles or borrowed pointers, including data-exchange events, monitor/controller events, pointer-bearing text-edit updates, and window-state snapshots that carry a borrowed icon handle.

### 2. Committing
When ready to process pushed events, call `maru_commitQueue()` on the queue creator thread. This freezes the active buffer as the stable snapshot, then clears the active buffer for the next frame.

### 3. Scanning
Call `maru_scanQueue()` to iterate the stable snapshot. Scanning can happen from another thread as long as it does not race with `maru_commitQueue()`.

## Threading and Synchronization

- `maru_pushQueue()` and `maru_commitQueue()` **MUST** be called from the queue creator thread.
- `maru_scanQueue()` may run on another thread, but you must externally synchronize it against `maru_commitQueue()`.
- `MARU_Queue` is not a lock-free SPMC queue. A read-write lock, barrier, or equivalent handoff is required if worker threads scan snapshots.

## C API Example

```c
#include <maru/maru.h>
#include <maru/queue.h>

static void queue_capture_cb(MARU_EventId type, MARU_Window *window,
                             const MARU_Event *evt, void *userdata) {
    MARU_Queue *queue = (MARU_Queue *)userdata;
    maru_pushQueue(queue, type,
                   window ? maru_getWindowId(window) : MARU_WINDOW_ID_NONE,
                   evt);
}

static void on_queue_event(MARU_EventId type, MARU_WindowId window_id,
                           const MARU_Event *evt, void *userdata) {
    (void)window_id;
    (void)evt;
    if (type == MARU_EVENT_CLOSE_REQUESTED) {
        *(bool *)userdata = false;
    }
}

int main() {
    MARU_Context *ctx = ...;
    MARU_Queue *queue = NULL;
    MARU_QueueCreateInfo queue_info = MARU_QUEUE_CREATE_INFO_DEFAULT;
    queue_info.capacity = 1024;
    maru_createQueue(&queue_info, &queue);

    bool running = true;
    while (running) {
        // 1. Pump queue-safe events and push each callback event into queue
        maru_pumpEvents(ctx, 16, MARU_QUEUE_SAFE_EVENT_MASK, queue_capture_cb,
                        queue);

        // 2. Publish snapshot
        maru_commitQueue(queue);

        // 3. Consume snapshot
        maru_scanQueue(queue, MARU_ALL_EVENTS, on_queue_event, &running);
    }

    maru_destroyQueue(queue);
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
maru_setQueueCoalesceMask(queue,
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
    auto queue_result = maru::Queue::create(1024);
    maru::Queue &queue = *queue_result;

    bool running = true;
    while (running) {
        ctx.pumpEvents(16, MARU_QUEUE_SAFE_EVENT_MASK, queue_capture_cb,
                       &queue);
        queue.commit();

        queue.scan(maru::overloads{
            [&](maru::CloseRequestedEvent) { running = false; },
            [&](maru::MouseMovedEvent e) { (void)e; }
        });
    }
}
```

## Performance Considerations

- **Memory Layout**: `MARU_Queue` uses a bulk-allocated, 64-byte aligned memory layout to minimize cache misses and false sharing during scanning.
- **Lock-Free**: The active/stable buffer swap in `commit()` is O(1) and designed for high throughput on the main thread.
- **Filtering**: Use `MARU_QUEUE_SAFE_EVENT_MASK` at pump-time and a scan mask in `maru_scanQueue()` to minimize queue traffic and scan work.
