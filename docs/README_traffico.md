[← Back to main README](../README.md)

# ▶️ `traffico.cpp` — Entry Point & Simulation Loop

This is the program's entry point. It handles startup, launches all threads, and waits for the simulation to complete.

---

## Startup Sequence

```
1. Read total_intersections and total_cars from stdin
2. initialize_intersections()   — create Intersection objects
3. initialize_cars()            — create Car objects, assign start positions
4. create_ambulances()          — create Ambulance objects (not yet launched)
5. launch_cars()                — start one thread per car
6. main thread waits on cv_continue
7. join_threads()               — join all car and ambulance threads
8. print per-car statistics
9. delete all vehicle objects
```

---

## `simulate_crossing` — Main Vehicle Loop

This is the core function run by every vehicle thread (both cars and ambulances). Each thread calls it once and runs to completion.

### Full Flow

```
1.  Record start time
2.  Sleep arrival_time seconds  (simulates traveling to the first intersection)
3.  Loop until all intersections have been crossed:
    a.  try_acquire the semaphore
    b.  Decrement free_spots under direction_mutex
    c.  Call set_directions to acquire exit_direction
    d.  Compute next = (idx + 1) % total_intersections
    e.  Call TrafficAlgorithm::start_algorithm to assign entry_direction for next intersection
    f.  Release semaphore, increment free_spots
    g.  Sleep crossing_time seconds
    h.  Call leave_intersection on current intersection
    i.  Sleep arrival_time seconds  (travel to next)
    j.  Call wait_in_queue on next intersection
    k.  Update idx and finish_last
    l.  Call run() — if true, call rush_to_hospital and break
4.  Call leave_intersection on the last intersection
5.  Call reset_ambulance()       (no-op for cars, resets percentage for ambulances)
6.  If ambulances_launched < ambulances.size(): call add_percentage on next ambulance
7.  Call launch_ambulance()
8.  Call exit_simulation()
9.  Call get_time()
```

### Loop Condition

```cpp
i < last_intersection || (finish_last != last_intersection)
```

- `i < last_intersection` counts intersections crossed (runs for the first `total_intersections - 1` crossings)
- `finish_last != last_intersection` catches vehicles that have not yet landed on the last intersection index, handling all starting positions correctly regardless of where the vehicle began

---

## Ambulance Launch (`launch_ambulance`)

Called by every vehicle after it completes `simulate_crossing`.

1. Returns immediately if `ambulances_launched == ambulances.size()` (all already launched)
2. Acquires both `ambulance_mutex` and `intersections.at(0)->direction_mutex` via `std::scoped_lock`
3. Generates a random threshold between 0.8 and 1.0
4. Checks:
   - `ambulances.at(ambulances_launched)->percentage >= threshold`
   - `intersections.at(0)->directions.at("Back") == true` (entry slot is free)
5. If both true:
   - Pushes the ambulance into `intersections.at(0)->vehicles_in_intersection`
   - Increments `ambulances_active` and `ambulances_launched`
   - Starts the ambulance thread running `simulate_crossing` from intersection 0

The ambulance always enters intersection 0 from the `"Back"` direction — set in the `Ambulance` constructor and verified in `launch_ambulance` before launching.

---

## Termination

The main thread waits on `cv_continue` with the predicate:

```cpp
cars_completed >= total_cars && ambulances_active == 0
```

After waking:
- `join_threads` is called on all cars and all ambulances (only launched ones have a joinable thread)
- Per-car statistics are printed via `Car::print_info()`
- All vehicle objects are deleted

Non-launched ambulances are never joined (their thread was never started) and simply have their memory freed.

---

## Build

```bash
# Single command
g++ -std=c++20 Intersection.cpp Traffic_algorithm.cpp traffico.cpp -o simulator

# Step by step (for low-memory systems / MinGW on Windows)
g++ -std=c++20 -c Intersection.cpp -o Intersection.o
g++ -std=c++20 -c Traffic_algorithm.cpp -o Traffic_algorithm.o
g++ -std=c++20 -c traffico.cpp -o traffico.o
g++ Intersection.o Traffic_algorithm.o traffico.o -o simulator
```

**Requirements:** C++20 · GCC or Clang · `-pthread` may be required on Linux

---

## Tests

Unit tests use [Google Test](https://github.com/google/googletest) and cover initial state of all four modules.

```bash
g++ -std=c++20 test.cpp Intersection.cpp Traffic_algorithm.cpp -o test_runner \
    -I"path/to/googletest/include" -L"path/to/googletest/lib" -lgtest -lgtest_main -pthread
./test_runner
```

```
[==========] 25 tests from 4 test suites ran.
[  PASSED  ] 25 tests.
```

### Test Coverage

- **Intersection** — initial state: all directions free, `free_spots` = 3, no vehicles, empty queues, `Forward` has no queue, 4 directions present
- **Globals** — initial state of atomic counters, `max_per_intersection`
- **Car** — name, start intersection, `queues_entered` = 0, `add_queue` increments correctly, timing ranges
- **Ambulance** — name, `entry_direction` = `"Back"`, `siren` = false, `percentage` = 0, `add_percentage` and `reset_ambulance` work correctly, timing and patient count ranges
