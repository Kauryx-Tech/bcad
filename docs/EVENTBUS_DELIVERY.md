# EventBus Delivery Guarantees and Filtering

## Delivery Model

The EventBus uses **synchronous, direct delivery**:

1. **Synchronous**: `publish()` blocks until all subscribers have been called
2. **Direct**: No event queue, no thread pool - callbacks execute on the caller's thread
3. **Ordered**: Subscribers are called in registration order (FIFO)
4. **No buffering**: Events are not stored - if no subscribers, event is dropped

## Thread Safety

- **Not thread-safe**: EventBus is not thread-safe by design
- Expected usage: Single-threaded (UI thread) or external synchronization
- For multi-threaded scenarios, wrap with mutex or use thread-local EventBus

## Delivery Guarantees

| Property | Guarantee |
|----------|-----------|
| Order | FIFO (registration order) |
| Blocking | Yes (synchronous) |
| Exceptions | Propagate up (subscriber exceptions crash publisher) |
| Duplicates | Possible if same subscriber registered multiple times |
| Lost events | Possible if no subscribers at publish time |

## Filtering

EventBus supports filtering via `EventFilter`:

```cpp
EventFilter filter;
filter.document = myDocument;           // Only events from this document
filter.entityTypes = {TypeId_Line};     // Only Line entity events
filter.entityIds = {42, 43};            // Only specific entity IDs
filter.layerName = "Walls";             // Only events on "Walls" layer

bus.subscribe<EntityAdded>(handler, filter);
```

### Filter Matching Logic

All filters are **AND-combined** (all must match):

| Filter | Matches When |
|--------|--------------|
| `document` | Event originates from same document pointer |
| `entityTypes` | Event's entity TypeId is in the list |
| `entityIds` | Event's entity ID is in the list |
| `layerName` | Event's entity layer matches exactly |

**All filters must match** (AND logic). Empty filter = match all.

## Performance

- **O(1)** publish for typed subscriptions (hash map lookup)
- **O(N)** for "all events" subscribers (N = count)
- Filter evaluation: O(1) per filter (hash lookups)

## Best Practices

1. **Use typed subscriptions** for performance (avoids "all events" overhead)
2. **Use filters** instead of checking in handler (avoids callback overhead)
3. **Use RAII guards** for automatic cleanup:
   ```cpp
   auto guard = bus.subscribe<EntityAdded>(handler, filter);
   // Auto-unsubscribes on scope exit
   ```
4. **Avoid heavy work** in handlers (blocks publisher)
5. **Don't publish from handlers** (can cause reentrancy issues)

## Example Usage

```cpp
// Subscribe with filter
EventFilter filter;
filter.document = myDoc;
filter.entityTypes = {TypeId_Line, TypeId_Circle};

auto guard = EventBus::instance().subscribe<EntityAdded>(
    [](const EntityAdded& e) { 
        std::cout << "Added: " << e.entity->typeId().value << std::endl;
    },
    filter
);

// Publish
EventBus::instance().publish(EntityAdded{doc, lineEntity});

// Auto-cleanup on scope exit
```

## Limitations

- No async delivery (use separate thread + queue if needed)
- No event persistence/replay
- No priority ordering
- No dead letter handling