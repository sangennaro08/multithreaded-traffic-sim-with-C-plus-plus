[← Back to main README](../MAIN_README.md)

# 🚗 `Vehicle` / `Car` / `Ambulance` — Class Hierarchy

```
Vehicle (abstract)
├── Car
└── Ambulance
```

---

## `Vehicle` — Abstract Base

Holds all fields shared by both vehicle types.

### Fields

| Field | Type | Description |
|---|---|---|
| `name` | `string` | Vehicle identifier |
| `start_intersection` | `atomic<int>` | Current intersection index (updated as vehicle moves) |
| `crossing_time` | `int` | Seconds to cross one intersection |
| `arrival_time` | `int` | Seconds to travel between intersections |
| `exit_direction` | `string` | Direction slot occupied in the current intersection |
| `entry_direction` | `string` | Direction slot requested in the next intersection (assigned by traffic algorithm) |
| `choosed` | `bool` | Set to `true` when the traffic algorithm has assigned a direction |
| `start`, `end` | `chrono::time_point` | For measuring total simulation time |
| `generator`, `distribution` | RNG | Per-vehicle RNG seeded with current time (uniform int 1–5) |
| `th` | `std::thread` | The vehicle's own thread |

### Virtual Interface

| Method | Overridden By | Notes |
|---|---|---|
| `exit_simulation()` | `Car`, `Ambulance` | Pure virtual — handles completion logic |
| `start_time()` | `Car`, `Ambulance` | Pure virtual — records start time point |
| `add_queue()` | `Car` | Default no-op for `Ambulance` |
| `get_time()` | `Car` | Default no-op for `Ambulance` |
| `add_percentage()` | `Ambulance` | Default no-op for `Car` |
| `reset_ambulance()` | `Ambulance` | Default no-op for `Car` |
| `run()` | `Ambulance` | Default no-op for `Car` |

---

## `Car`

- `crossing_time` = `random(1–5) × random(1–5)` seconds
- `arrival_time` = `random(1–5) × 3` seconds
- `queues_entered` — `atomic<int>`, incremented each time the car has to wait in an external queue

### `exit_simulation()`

Prints completion message, increments `cars_completed`, notifies `cv_continue` if all cars are done and no ambulances are active.

### `print_info()`

Static method printing: name, start intersection, total time elapsed, and queues entered.

---

## `Ambulance`

- `crossing_time` = `random(1–5) + 2` seconds
- `arrival_time` = `random(1–5) + 2` seconds
- `patients` = `random(1–5)` — used to compute the emergency threshold
- `entry_direction` is always initialized to `"Back"` in the constructor

### Fields

| Field | Type | Description |
|---|---|---|
| `patients` | `int` | Number of patients; higher = lower emergency time threshold |
| `percentage` | `atomic<float>` | Launch probability, incremented by 0.1 per completed vehicle |
| `siren` | `bool` | Currently unused in logic, reserved for future use |

### `run()`

Checks elapsed time since `start_time()` was called. Returns `true` if:

```
time_in_simulation > 120 / patients
```

| patients | threshold |
|---|---|
| 1 | 120 seconds |
| 2 | 60 seconds |
| 3 | 40 seconds |
| 5 | 24 seconds |

More patients = more urgency = earlier emergency trigger.

### `reset_ambulance()`

Resets `percentage` to `0.0`. Called at the start of the ambulance's own `simulate_crossing` via polymorphic dispatch (no-op for cars). This prevents accumulated probability from carrying over.

### `exit_simulation()`

Decrements `ambulances_active`, prints message, notifies `cv_continue` if all cars are done and no ambulances remain active.

---

## Ambulance Launch Flow

Ambulances are **not** launched at startup. Instead, each time any vehicle finishes `simulate_crossing`:

1. It calls `add_percentage` on `ambulances.at(ambulances_launched)` — always the **next ambulance in line** — incrementing its probability by `0.1`
2. Then calls `launch_ambulance`, which acquires both `ambulance_mutex` and `intersections.at(0)->direction_mutex` via `std::scoped_lock`
3. Generates a random threshold between 0.8 and 1.0
4. Checks two conditions:
   - `ambulances.at(ambulances_launched)->percentage >= threshold`
   - `intersections.at(0)->directions.at("Back") == true` (the `"Back"` slot at intersection 0 is free)
5. If both are true: pushes the ambulance into `intersections.at(0)->vehicles_in_intersection`, increments `ambulances_active` and `ambulances_launched`, and starts the thread from intersection 0

The ambulance always enters intersection 0 from the `"Back"` direction — set in the constructor and checked in `launch_ambulance` before launching.

---

## Design Decisions

### Raw pointers for vehicles

`Car` and `Ambulance` objects are stored as `Vehicle*`. Using `unique_ptr<Vehicle>` with polymorphism creates complications: `unique_ptr` cannot be copied, making it awkward to insert the same vehicle pointer into multiple data structures (the cars vector, `vehicles_in_intersection`, etc.). Since object lifetimes are fully explicit — all vehicles are created before threads start and deleted after all threads have joined in `main` — there is no risk of leaks or dangling pointers. Raw pointers avoid the overhead and restrictions of smart pointers at no safety cost here.

### `ambulances_launched` as both counter and index

Rather than erasing ambulances from the vector when launched (which would invalidate indices and shift elements), `ambulances_launched` is kept as a monotonically increasing index. The ambulance at position `ambulances_launched` is always the next one to be launched. This also allows `join_threads` to simply iterate the full ambulances vector and join any thread that is joinable, without needing a separate tracking structure.

### `scoped_lock` with two mutexes in `launch_ambulance`

The launch check reads `percentage` (needs `ambulance_mutex`) and reads `directions` (needs `direction_mutex`). Acquiring them separately would risk deadlock if another thread holds `direction_mutex` and tries to acquire `ambulance_mutex`. `scoped_lock` acquires both atomically using a deadlock-avoidance algorithm, making acquisition order irrelevant.

If both are true, the ambulance thread is started from intersection 0 via the `"Back"` direction.

`ambulances_launched` acts as both a counter and a stable index — ambulances are never removed from the vector, avoiding index invalidation and allowing `join_threads` to simply iterate and join any joinable thread.
