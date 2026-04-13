[← Back to main README](../README.md)

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
| `entry_direction` | `string` | Direction slot assigned for the next intersection |
| `choosed` | `bool` | Set to `true` when the traffic algorithm has assigned a direction |
| `start`, `end` | `chrono::time_point` | For measuring total simulation time |
| `generator`, `distribution` | RNG | Per-vehicle RNG seeded with current time (uniform int 1–5) |
| `th` | `std::thread` | The vehicle's own thread |

### Virtual Interface

| Method | Overridden By | Purpose |
|---|---|---|
| `exit_simulation()` | `Car`, `Ambulance` | Completion logic (pure virtual) |
| `start_time()` | `Car`, `Ambulance` | Records start time point (pure virtual) |
| `add_queue()` | `Car` | Increments `queues_entered` counter |
| `get_time()` | `Car` | Prints total simulation time |
| `add_percentage()` | `Ambulance` | Increments launch probability |
| `reset_ambulance()` | `Ambulance` | Resets `percentage` to 0 |
| `run()` | `Ambulance` | Returns `true` if emergency threshold exceeded |

`add_percentage`, `reset_ambulance`, and `run` default to no-ops in `Car` — calling them on a car pointer is safe and does nothing.

---

## `Car`

- `crossing_time` = `random(1–5) × random(1–5)` seconds
- `arrival_time` = `random(1–5) × 3` seconds
- `queues_entered` — `atomic<int>`, incremented each time the car has to wait in an external queue

### `exit_simulation()`
Prints a completion message, increments `cars_completed`, and notifies `cv_continue` if all cars are done and no ambulances are still active.

### `print_info()`
Static method printing: name, start intersection, total time elapsed, and number of queues entered.

---

## `Ambulance`

- `crossing_time` = `random(1–5) + 2` seconds
- `arrival_time` = `random(1–5) + 2` seconds
- `patients` = `random(1–5)` — determines the emergency threshold
- `entry_direction` is always initialized to `"Back"` in the constructor

### Fields

| Field | Type | Description |
|---|---|---|
| `patients` | `int` | Number of patients; higher = lower emergency threshold |
| `percentage` | `atomic<float>` | Launch probability, incremented by 0.1 per completed vehicle |
| `siren` | `bool` | Reserved for future use (currently unused in logic) |

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
Resets `percentage` to `0.0`. Called at the start of `simulate_crossing` via polymorphic dispatch (no-op for cars). This prevents the ambulance's accumulated probability from carrying over if it is somehow relaunched.

### `exit_simulation()`
Decrements `ambulances_active`, prints a message, and notifies `cv_continue` if all cars are done and no ambulances remain active.

---

## Why Raw Pointers?

`Car` and `Ambulance` objects are stored as `Vehicle*`. Using `unique_ptr<Vehicle>` with polymorphism creates complications: `unique_ptr` cannot be copied, making it awkward to insert the same vehicle pointer into multiple data structures (the cars vector, `vehicles_in_intersection`, etc.).

Since object lifetimes are fully explicit — all vehicles are created before threads start and deleted after all threads have joined in `main` — there is no risk of leaks or dangling pointers. Raw pointers avoid the overhead and restrictions of smart pointers at no safety cost here.

---

## Ambulance Launch Logic

Ambulances are not launched at startup. Instead, each time any vehicle finishes `simulate_crossing`, it calls `add_percentage` on the **next ambulance in line** (indexed by `ambulances_launched`), incrementing its probability by `0.1`.

Then `launch_ambulance` is called, which checks:
1. `ambulances.at(ambulances_launched)->percentage >= threshold` (random 0.8–1.0)
2. `intersections.at(0)->directions.at("Back") == true` (entry slot is free)

If both are true, the ambulance thread is started from intersection 0 via the `"Back"` direction.

`ambulances_launched` acts as both a counter and a stable index — ambulances are never removed from the vector, avoiding index invalidation and allowing `join_threads` to simply iterate and join any joinable thread.
