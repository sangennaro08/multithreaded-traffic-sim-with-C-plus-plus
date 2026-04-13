[← Back to main README](../MAIN_README.md)

# 📦 `globals.hpp` — Global State

This header defines all shared state accessible across every translation unit in the simulation. It is included by `traffico.cpp`, `Intersection.cpp`, and `Traffic_algorithm.cpp`.

---

## Variables

| Variable | Type | Purpose |
|---|---|---|
| `total_intersections` | `int` | Number of intersections in the chain (set at runtime) |
| `total_cars` | `int` | Number of cars to simulate (set at runtime) |
| `max_per_intersection` | `constexpr int` | Max vehicles allowed simultaneously per intersection (3) |
| `cars_completed` | `atomic<int>` | Counter incremented when each car exits the last intersection |
| `ambulances_active` | `atomic<int>` | Number of ambulances currently running in the simulation |
| `ambulances_launched` | `atomic<int>` | Index of the next ambulance to be launched from the ambulances vector |
| `print_mutex` | `mutex` | Serializes all console output across threads |
| `end_program` | `mutex` | Used exclusively with `cv_continue` for main thread waiting |
| `ambulance_mutex` | `mutex` | Protects concurrent access to ambulance `percentage` and the launch logic |
| `cv_continue` | `condition_variable` | Wakes main thread when all cars and ambulances have finished |

---

## Termination Predicate

The main thread waits on `cv_continue` with the predicate:

```cpp
cars_completed >= total_cars && ambulances_active == 0
```

This means the simulation does not end until every car has exited the last intersection **and** every launched ambulance has also finished. Non-launched ambulances are never counted in `ambulances_active` and do not block termination.

---

## Design Decisions

### `percentage` is both `atomic<float>` and protected by `ambulance_mutex`

`percentage` is `atomic<float>` for cheap lock-free reads. However `add_percentage` and `reset_ambulance` still take `ambulance_mutex` because those operations need to be coordinated with the launch check in `launch_ambulance`, which also holds `ambulance_mutex`. This prevents a race where a percentage increment and a launch decision happen simultaneously.

### `ambulances_launched` as both counter and index

Rather than erasing ambulances from the vector when launched (which would invalidate indices and shift elements), `ambulances_launched` is kept as a monotonically increasing index. The ambulance at position `ambulances_launched` is always the next one to be launched. This also allows `join_threads` to simply iterate the full ambulances vector and join any thread that is joinable, without needing a separate tracking structure.

### `print_mutex` is intentionally isolated

It is never held while waiting on I/O or other resources, which would stall console output across the entire simulation.

---

## Thread Safety Summary

Every mutable global is either `atomic` (for simple counters) or protected by a dedicated mutex. There are no bare global variables written from multiple threads without synchronization.
y Summary

Every mutable global is either `atomic` or protected by a dedicated mutex. There are no bare global variables written from multiple threads without synchronization.
