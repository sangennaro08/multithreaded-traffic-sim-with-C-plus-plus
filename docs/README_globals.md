[← Back to main README](../MAIN_README.md)

# 📦 `globals.hpp` — Global State

This header defines all shared state accessible across every translation unit in the simulation. It is included by `traffico.cpp`, `Intersection.cpp`, and `Traffic_algorithm.cpp`.

---

## Variables

| Variable | Type | Purpose |
|---|---|---|
| `total_intersections` | `int` | Number of intersections in the chain (set at runtime) |
| `total_cars` | `int` | Number of car threads to simulate (set at runtime) |
| `max_per_intersection` | `constexpr int` (= 3) | Hard cap on vehicles simultaneously inside one intersection |
| `cars_completed` | `atomic<int>` | Incremented by each car when it exits the last intersection |
| `ambulances_active` | `atomic<int>` | Number of ambulances currently running |
| `ambulances_launched` | `atomic<int>` | Index of the next ambulance to be launched from the vector |
| `print_mutex` | `mutex` | Serializes all `std::cout` output across threads |
| `end_program` | `mutex` | Paired exclusively with `cv_continue` for main-thread waiting |
| `ambulance_mutex` | `mutex` | Protects ambulance `percentage` reads/writes and launch decision |
| `cv_continue` | `condition_variable` | Wakes the main thread when all cars and ambulances have finished |

---

## Why `atomic` vs `mutex`?

- **`cars_completed`, `ambulances_active`, `ambulances_launched`** are `atomic<int>` because they only need simple increment/read operations — no mutex needed, lower overhead.
- **`ambulance_mutex`** is still required even though `percentage` is `atomic<float>`, because the launch decision combines a read of `percentage` with a write to `directions` — two separate operations that must appear atomic together. A mutex handles this compound check.
- **`print_mutex`** is intentionally isolated from all other locks. Holding it while waiting on I/O or other resources would stall console output across the entire simulation.

---

## Termination Predicate

The main thread waits on `cv_continue` using:

```cpp
cars_completed >= total_cars && ambulances_active == 0
```

This means the simulation does not end until **every car has exited** the last intersection **and every launched ambulance** has also finished. Non-launched ambulances (those that never reached their probability threshold) are not counted in `ambulances_active` and do not block termination.

---

## Thread Safety Summary

Every mutable global is either `atomic` or protected by a dedicated mutex. There are no bare global variables written from multiple threads without synchronization.
