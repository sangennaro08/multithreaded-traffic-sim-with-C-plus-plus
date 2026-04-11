[Uploading UML structure.xml…]()
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()

# Traffic Simulator

A multithreaded C++ traffic simulation where cars and ambulances navigate through a chain of intersections using real concurrency primitives — mutexes, semaphores, and condition variables.

---

## Overview

Multiple car threads travel through a fixed chain of intersections in sequential order. Each intersection is a shared resource: vehicles compete for directional slots, queue externally when a slot is occupied, and proceed only when it is free. Ambulances are created at startup but launched dynamically during the simulation based on a probability threshold. If an ambulance spends too long in transit, it enters emergency rush mode and bypasses all normal traffic rules.

## 📐 Architecture

### UML Class Diagram

![UML Class Diagram](./docs/uml.png)

---

## Project Structure

```
├── globals.hpp                  # Global state: counters, mutexes, condition variable
├── traffico.cpp                 # Entry point, simulation setup and teardown
├── Intersection.cpp/.hpp        # Intersection logic, car/ambulance initialization, thread management
├── Traffic_algorithm.cpp/.hpp   # Direction assignment algorithm, emergency rush logic
└── vehicles/
    ├── vehicle.hpp              # Abstract base class for all vehicles
    ├── car.hpp                  # Car implementation
    └── ambulance.hpp            # Ambulance implementation
```

---

## Global State (`globals.hpp`)

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

## Class Hierarchy

```
Vehicle (abstract)
├── Car
└── Ambulance
```

### `Vehicle` (abstract base)
Holds all fields shared by both vehicle types:
- `name` — vehicle identifier
- `start_intersection` — atomic int, current intersection index (updated as vehicle moves)
- `crossing_time` — seconds to cross one intersection
- `arrival_time` — seconds to travel between intersections
- `exit_direction` — direction slot occupied in the current intersection
- `entry_direction` — direction slot requested in the next intersection (assigned by traffic algorithm)
- `choosed` — flag set to true when the traffic algorithm has assigned a direction
- `start`, `end` — chrono time points for measuring total simulation time
- `generator`, `distribution` — per-vehicle RNG seeded with current time (uniform int 1–5)
- `th` — the vehicle's thread

Virtual interface:
- `exit_simulation()` = 0 — pure virtual, handles completion logic
- `start_time()` = 0 — pure virtual, records start time point
- `add_queue()`, `get_time()` — overridden by `Car`
- `add_percentage()`, `reset_ambulance()`, `run()` — overridden by `Ambulance`; default no-ops for `Car`

### `Car`
- `crossing_time` = random(1–5) × random(1–5) seconds
- `arrival_time` = random(1–5) × 3 seconds
- `queues_entered` — atomic counter incremented each time the car has to wait in an external queue
- `exit_simulation()` — prints completion message, increments `cars_completed`, notifies `cv_continue` if all cars are done and no ambulances are active
- `print_info()` — static method printing name, start intersection, total time, and queues entered

### `Ambulance`
- `crossing_time` = random(1–5) + 2 seconds
- `arrival_time` = random(1–5) + 2 seconds
- `patients` = random(1–5), used to compute the emergency threshold
- `percentage` — atomic float, incremented by 0.1 each time any vehicle completes the simulation; protected also by `ambulance_mutex` in `add_percentage` and `reset_ambulance`
- `siren` — bool flag (currently unused in logic, reserved for future use)
- `entry_direction` is always initialized to `"Back"` in the constructor
- `run()` — checks elapsed time since `start_time()` was called; returns true if `time_in_simulation > 120 / patients`. With 1 patient the threshold is 120s, with 5 patients it is 24s
- `reset_ambulance()` — resets `percentage` to 0.0, called at the start of the ambulance's own `simulate_crossing` via polymorphic dispatch (no-op for cars)
- `exit_simulation()` — decrements `ambulances_active`, prints message, notifies `cv_continue` if all cars are done and no ambulances remain active

---

## Intersection

Each `Intersection` object holds:
- `enter` — `counting_semaphore<3>`, limits simultaneous vehicles to `max_per_intersection`
- `free_spots` — int mirroring available capacity, updated under `direction_mutex`
- `directions` — `unordered_map<string, bool>` with four slots: `"Right"`, `"Left"`, `"Back"`, `"Forward"`. `true` = available, `false` = occupied
- `entry_queues` — `unordered_map<string, queue<Vehicle*>>` with three queues: `"Right"`, `"Left"`, `"Back"`. `"Forward"` has no queue because vehicles exiting forward move to the next intersection in the chain, not waiting at the current one
- `vehicles_in_intersection` — vector of pointers to vehicles currently inside
- `direction_mutex` — protects `directions`, `free_spots`, and `vehicles_in_intersection`
- `find_pattern` — mutex used by the traffic algorithm to serialize direction assignment
- `get_direction_cv` — condition variable for waiting vehicles to be notified when a direction has been assigned
- `exit_cv` — condition variable notified when a vehicle leaves, used by queued vehicles to re-check their slot

---

## Initialization

### `initialize_intersections`
Creates `total_intersections` `Intersection` objects and stores them as `unique_ptr` in the vector.

### `initialize_cars`
Creates `total_cars` `Car` objects. Each car is assigned a starting intersection via `pick_intersection`, which randomly samples intersections until one with fewer than 3 vehicles is found. The car is pushed into that intersection's `vehicles_in_intersection` immediately.

### `create_ambulances`
Creates between 1 and `(total_cars / 8) + 1` `Ambulance` objects. They are stored in the ambulances vector but **not launched** — they wait until the simulation triggers them via `launch_ambulance`. The number of ambulances scales with the number of cars.

### `launch_cars`
Starts one `std::thread` per car, each running `simulate_crossing` on the `Intersection` object at the car's `start_intersection`.

---

## `simulate_crossing` — Main Vehicle Loop

This is the core function run by every vehicle thread (both cars and ambulances).

```
1. Record start time
2. Sleep arrival_time seconds (simulates traveling to the first intersection)
3. Loop until all intersections have been crossed and finish_last == last_intersection:
   a. try_acquire the semaphore (always succeeds since max 3 cars per intersection is enforced during initialization)
   b. Decrement free_spots under direction_mutex
   c. Call set_directions to assign exit_direction
   d. Compute next = (idx + 1) % total_intersections
   e. Call TrafficAlgorithm::start_algorithm to assign entry_direction for next intersection
   f. Release semaphore and increment free_spots
   g. Sleep crossing_time seconds
   h. Call leave_intersection on current intersection
   i. Sleep arrival_time seconds (travel to next)
   j. Call wait_in_queue on next intersection
   k. Update idx and finish_last
   l. Call run() — if true, call rush_to_hospital and break
4. Call leave_intersection on the last intersection
5. Call reset_ambulance (no-op for cars, resets percentage to 0 for ambulances)
6. If ambulances_launched < ambulances.size(), call add_percentage on the next ambulance in line
7. Call launch_ambulance
8. Call exit_simulation
9. Call get_time
```

**Loop condition:** `i < last_intersection || (finish_last != last_intersection)`
- `i < last_intersection` counts intersections crossed. Since `last_intersection = total_intersections - 1`, this runs for the first `total_intersections - 1` crossings.
- `finish_last != last_intersection` catches any vehicle that has not yet landed on the last intersection index regardless of `i`, handling all starting positions correctly.

---

## `set_directions`

Called under `direction_mutex`.

- If `entry_direction` is empty (first intersection or car with no pre-assigned direction): iterates `directions` and takes the **first available** slot, sets `exit_direction` to it and marks it `false`.
- If `entry_direction` is already set (assigned by traffic algorithm): copies it to `exit_direction` and clears `entry_direction`.

---

## `wait_in_queue`

Called for every intersection that is not the final one.

Acquires `direction_mutex` on the next intersection. If the requested `entry_direction` slot is already occupied (`directions.at(entry_direction) == false`):
1. Pushes the vehicle into the corresponding `entry_queues` queue
2. Increments `queues_entered` via `add_queue()`
3. Waits on `exit_cv` until two conditions are simultaneously true:
   - The vehicle is at the front of its queue
   - The requested direction slot is free
4. Pops itself from the queue

After waiting (or immediately if the slot was free), pushes the vehicle into `vehicles_in_intersection` and marks the direction as occupied.

---

## `leave_intersection`

Acquires `direction_mutex`. Removes the vehicle from `vehicles_in_intersection`, marks `exit_direction` as available again (`true`), and calls `exit_cv.notify_all()` to wake all vehicles waiting in queues on this intersection.

---

## Traffic Algorithm (`TrafficAlgorithm`)

### `start_algorithm`
Runs in a `do-while` loop until `V->choosed` is true.

Each iteration:
1. Acquires `find_pattern` mutex on the current intersection
2. If `can_decide` is true and the vehicle has not been assigned yet: sets `can_decide = false`, calls `search_exit`, restores `can_decide = true`, notifies `get_direction_cv`
3. Otherwise: waits on `get_direction_cv` until `V->choosed` is true
4. Increments `attempts` and calls `go_alone`

### `search_exit`
Acquires `direction_mutex` on both current and next intersection simultaneously.

Computes `to_move = min(next->free_spots, current->vehicles_in_intersection.size())`.

For each of the first `to_move` vehicles in `vehicles_in_intersection`, iterates `next->directions` and assigns the first available slot that is not `"Forward"` and not already assigned to this vehicle. Sets `choosed = true` on each assigned vehicle.

This function assigns directions to multiple vehicles at once in a single critical section, minimizing contention.

### `go_alone`
If `attempts >= 3` and the vehicle still has no direction: acquires `direction_mutex` on next intersection, picks a random direction from the map (excluding `"Forward"`), assigns it regardless of availability, and sets `choosed = true`. This is a fallback to prevent starvation when `search_exit` cannot find a slot.

---

## Ambulance Launch (`launch_ambulance`)

Called by every vehicle after it completes `simulate_crossing`.

1. Returns immediately if `ambulances_launched == ambulances.size()` (all ambulances already launched)
2. Acquires both `ambulance_mutex` and `intersections.at(0)->direction_mutex` simultaneously via `std::scoped_lock`
3. Generates a random threshold between 0.8 and 1.0
4. Checks two conditions:
   - `ambulances.at(ambulances_launched)->percentage >= threshold`
   - `intersections.at(0)->directions.at("Back") == true` (the "Back" slot at intersection 0 is free)
5. If both are true:
   - Pushes the ambulance into `intersections.at(0)->vehicles_in_intersection`
   - Increments `ambulances_active` and `ambulances_launched`
   - Launches the ambulance thread running `simulate_crossing` starting from intersection 0

The ambulance always enters intersection 0 from the `"Back"` direction. This is set in the `Ambulance` constructor (`entry_direction = "Back"`) and checked in `launch_ambulance` before launching. The `"Back"` direction at intersection 0 must be free for the launch to proceed.

`ambulances_launched` serves as both a counter and an index into the ambulances vector. The ambulances are launched in order (index 0, then 1, then 2…). Each time a vehicle finishes, it calls `add_percentage` on `ambulances.at(ambulances_launched)` — always the next ambulance in line — incrementing its probability by 0.1.

---

## Emergency Rush (`rush_to_hospital`)

Triggered when `run()` returns true — i.e. elapsed time exceeds `120 / patients` seconds.

While `idx != last_intersection`:
1. Calls `set_directions` on current intersection (acquires direction slot normally)
2. Sleeps `crossing_time / 2` seconds (half normal crossing time)
3. Calls `leave_intersection` on current intersection
4. Sleeps `arrival_time / 2` seconds (half normal travel time)
5. Pushes directly into `next->vehicles_in_intersection` under `direction_mutex` — **no semaphore, no queue, no free_spots check**
6. Updates `idx`

The ambulance bypasses the semaphore and queuing entirely, simulating a real emergency vehicle overriding traffic. The `direction_mutex` is still acquired when pushing into `vehicles_in_intersection` because other threads may be reading or modifying that vector concurrently.

---

## Termination

The main thread waits on `cv_continue` with the predicate:
```cpp
cars_completed >= total_cars && ambulances_active == 0
```

This means the simulation does not end until every car has exited the last intersection **and** every launched ambulance has also finished. After waking, the main thread calls `join_threads` on all cars and all ambulances (only launched ones have a joinable thread), prints per-car statistics, and deletes all vehicle objects.

---

## Concurrency Summary

| Primitive | Where | Purpose |
|---|---|---|
| `counting_semaphore<3>` | `Intersection::enter` | Hard limit of 3 vehicles per intersection |
| `mutex` + `lock_guard` | `direction_mutex` | Protects `directions`, `free_spots`, `vehicles_in_intersection` |
| `mutex` + `lock_guard` | `print_mutex` | Serializes all console output |
| `mutex` + `lock_guard` | `ambulance_mutex` | Protects `percentage` reads/writes and ambulance launch |
| `scoped_lock` | `launch_ambulance` | Acquires `ambulance_mutex` + `direction_mutex` together without deadlock risk |
| `unique_lock` + `condition_variable` | `wait_in_queue` | Blocks vehicle until its queue slot is at front and direction is free |
| `unique_lock` + `condition_variable` | `start_algorithm` | Blocks vehicle until direction has been assigned by `search_exit` |
| `unique_lock` + `condition_variable` | `main` | Blocks until all cars and ambulances are done |
| `atomic<int>` | `cars_completed`, `ambulances_active`, `ambulances_launched` | Lock-free counters shared across threads |
| `atomic<float>` | `Ambulance::percentage` | Lock-free reads of percentage value |

---

## Design Decisions

### Raw pointers for vehicles
`Car` and `Ambulance` objects are stored as `Vehicle*`. Using `unique_ptr<Vehicle>` with polymorphism creates complications: `unique_ptr` cannot be copied, making it awkward to insert the same vehicle pointer into multiple data structures (the cars vector, `vehicles_in_intersection`, etc.). Since object lifetimes are fully explicit — all vehicles are created before threads start and deleted after all threads have joined in `main` — there is no risk of leaks or dangling pointers. Raw pointers avoid the overhead and restrictions of smart pointers at no safety cost here.

### `ambulances_launched` as both counter and index
Rather than erasing ambulances from the vector when launched (which would invalidate indices and shift elements), `ambulances_launched` is kept as a monotonically increasing index. The ambulance at position `ambulances_launched` is always the next one to be launched. This also allows `join_threads` to simply iterate the full ambulances vector and join any thread that is joinable, without needing a separate tracking structure.

### `percentage` is both `atomic<float>` and protected by `ambulance_mutex`
`percentage` is `atomic<float>` for cheap lock-free reads. However `add_percentage` and `reset_ambulance` still take `ambulance_mutex` because those operations need to be coordinated with the launch check in `launch_ambulance`, which also holds `ambulance_mutex`. This prevents a race where a percentage increment and a launch decision happen simultaneously.

### `scoped_lock` with two mutexes in `launch_ambulance`
The launch check reads `percentage` (needs `ambulance_mutex`) and reads `directions` (needs `direction_mutex`). Acquiring them separately would risk deadlock if another thread holds `direction_mutex` and tries to acquire `ambulance_mutex`. `scoped_lock` acquires both atomically using a deadlock-avoidance algorithm, making acquisition order irrelevant.

### `"Forward"` has no entry queue
The `entry_queues` map only contains `"Right"`, `"Left"`, and `"Back"`. `"Forward"` is excluded because a vehicle moving forward is traveling to the next intersection in the chain — it does not need to queue at the current one. The traffic algorithm also excludes `"Forward"` from direction assignments (`dir != "Forward"` in `search_exit` and `go_alone`).

### `try_acquire` instead of `acquire` on the semaphore
`acquire()` would block the thread indefinitely. `try_acquire()` returns immediately if the semaphore count is zero, allowing the for loop to retry. In practice the semaphore never blocks because `pick_intersection` ensures at most 3 cars start at any intersection, and cars move through sequentially — but `try_acquire` is the correct non-blocking pattern for a loop that needs to keep re-evaluating its exit condition.

---

## Build

```bash
g++ -std=c++20 Intersection.cpp Traffic_algorithm.cpp traffico.cpp -o simulator
```

If compilation runs out of memory (common on Windows with MinGW), compile separately:

```bash
g++ -std=c++20 -c Intersection.cpp -o Intersection.o
g++ -std=c++20 -c Traffic_algorithm.cpp -o Traffic_algorithm.o
g++ -std=c++20 -c traffico.cpp -o traffico.o
g++ Intersection.o Traffic_algorithm.o traffico.o -o simulator
```

## Requirements

- C++20 or later (requires `std::counting_semaphore` and `std::scoped_lock`)
- GCC or Clang with threading support (`-pthread` may be required on Linux)
