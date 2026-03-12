# Metrics & Instrumentation

Maru provides built-in metrics and instrumentation to help you monitor the health and performance of your application.

## Metrics

Maru objects (currently only the Context) collect various runtime statistics. These are accessible through a "getter" function:

- `maru_getContextMetrics(context)`

### Context Metrics

The `MARU_ContextMetrics` struct provides a high-level view of the library's state:

- **Memory**: Current and peak allocated memory.
- **Cursors**: Total and current number of created/alive cursors (system and custom).
- **Performance**: Number of calls to `maru_pumpEvents` and average/peak processing time in nanoseconds.
- **User Events**: Statistics on the internal event queue, including peak count and drop counts (if the queue overflows).

```c
const MARU_ContextMetrics *metrics = maru_getContextMetrics(context);
printf("Current memory allocation: %lu bytes\n", metrics->memory_allocated_current);
printf("Peak event queue count: %u\n", metrics->user_events->peak_count);
```

### Resetting Metrics

Most metrics are cumulative. You can reset them using:

- `maru_resetContextMetrics(context)`

---

## Diagnostics

Maru uses a diagnostic callback to report errors, warnings, and informational messages. You provide this callback when creating a context.

```c
static void handle_diagnostic(const struct MARU_DiagnosticInfo *info, void *userdata) {
    const char* severity = "INFO";
    if (info->diagnostic >= MARU_DIAGNOSTIC_INVALID_ARGUMENT) {
        severity = "ERROR";
    } else if (info->diagnostic > MARU_DIAGNOSTIC_INFO) {
        severity = "WARNING";
    }
    
    fprintf(stderr, "[MARU %s] %s\n", severity, info->message);
}

// ... in main ...
MARU_ContextCreateInfo create_info = MARU_CONTEXT_CREATE_INFO_DEFAULT;
create_info.attributes.diagnostic_cb = handle_diagnostic;
```

This is the most effective way to debug issues like "Why won't my window open?" or "Why did my Vulkan surface creation fail?".

---

## Instrumentation (Development)

Maru includes internal instrumentation points that can be integrated with external profilers. This is primarily used for library development but can be useful for advanced performance analysis of your own applications.

Next: [X11 Backend](X11.md) or [Wayland Backend](wayland.md)
