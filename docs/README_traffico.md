[← Back to main README](../MAIN_README.md)

# ▶️ `traffico.cpp` — Entry Point & Simulation Loop

This is the program's entry point. It handles startup, launches all threads, runs the main vehicle loop, and waits for the simulation to complete.

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
8. print per-car statistics via Car::print_info()
9. delete all vehicle objects
```

---

## `simulate_crossing` — Main Vehicle Loop

This is the core function run by every vehicle thread (both cars and ambulances). Each thread calls it once and runs to completion.

```
1.  Record start time
2.  Sleep arrival_time seconds  (simulates traveling to the first intersection)
3.  Loop until all intersections have been crossed and finish_last == last_intersection:
    a.  try_acquire the semaphore (always succeeds since max 3 cars per intersection is enforced during initialization)
    b.  Decrement free_spots under direction_mutex
    c.  Call set_directions to assign exit_direction
    d.  Compute next = (idx + 1) % total_intersections
    e.  Call TrafficAlgorithm::start_algorithm to assign entry_direction for next intersection
    f.  Release semaphore and increment free_spots
    g.  Sleep crossing_time seconds
    h.  Call leave_intersection on current intersection
    i.  Sleep arrival_time seconds  (travel to next)
    j.  Call wait_in_queue on next intersection
    k.  Update idx and finish_last
    l.  Call run() — if true, call rush_to_hospital and break
4.  Call leave_intersection on the last intersection
5.  Call reset_ambulance()       (no-op for cars, resets percentage to 0 for ambulances)
6.  If ambulances_launched < ambulances.size(): call add_percentage on next ambulance in line
7.  Call launch_ambulance()
8.  Call exit_simulation()
9.  Call get_time()
```

### Loop Condition

```cpp
i < last_intersection || (finish_last != last_intersection)
```

- `i < last_intersection` counts intersections crossed. Since `last_intersection = total_intersections - 1`, this runs for the first `total_intersections - 1` crossings.
- `finish_last != last_intersection` catches any vehicle that has not yet landed on the last intersection index regardless of `i`, handling all starting positions correctly.

---

## Termination

The main thread waits on `cv_continue` with the predicate:

```cpp
cars_completed >= total_cars && ambulances_active == 0
```

This means the simulation does not end until every car has exited the last intersection **and** every launched ambulance has also finished. After waking, the main thread calls `join_threads` on all cars and all ambulances (only launched ones have a joinable thread), prints per-car statistics, and deletes all vehicle objects.

---

## Build

```bash
# Single command
g++ -std=c++20 Intersection.cpp Traffic_algorithm.cpp traffico.cpp -o simulator
```

If compilation runs out of memory (common on Windows with MinGW), compile separately:

```bash
g++ -std=c++20 -c Intersection.cpp -o Intersection.o
g++ -std=c++20 -c Traffic_algorithm.cpp -o Traffic_algorithm.o
g++ -std=c++20 -c traffico.cpp -o traffico.o
g++ Intersection.o Traffic_algorithm.o traffico.o -o simulator
```

**Requirements:** C++20 or later · GCC or Clang with threading support · `-pthread` may be required on Linux

---

## 🧪 Testing

Unit tests are written using [Google Test](https://github.com/google/googletest).

### Build and run tests

Requires [Google Test](https://github.com/google/googletest) compiled with MinGW.

```bash
g++ -std=c++20 test.cpp Intersection.cpp Traffic_algorithm.cpp -o test_runner \
    -I"path/to/googletest/include" -L"path/to/googletest/lib" -lgtest -lgtest_main -pthread
./test_runner
```

### Result

```
[==========] 25 tests from 4 test suites ran.
[  PASSED  ] 25 tests.
```

### Test coverage

- **Intersection** — initial state: all directions free, `free_spots` = 3, no vehicles, empty queues, `Forward` has no queue, 4 directions present
- **Globals** — initial state of atomic counters, `max_per_intersection`
- **Car** — name, start intersection, `queues_entered` starts at 0, `add_queue` increments correctly, `crossing_time` and `arrival_time` within expected ranges
- **Ambulance** — name, `entry_direction` is `"Back"`, `siren` starts false, `percentage` starts at 0, `add_percentage` increments correctly, `reset_ambulance` resets to 0, `patients` and timing within expected ranges

### Test Coverage

- **Intersection** — initial state: all directions free, `free_spots` = 3, no vehicles, empty queues, `Forward` has no queue, 4 directions present
- **Globals** — initial state of atomic counters, `max_per_intersection`
- **Car** — name, start intersection, `queues_entered` = 0, `add_queue` increments correctly, timing ranges
- **Ambulance** — name, `entry_direction` = `"Back"`, `siren` = false, `percentage` = 0, `add_percentage` and `reset_ambulance` work correctly, timing and patient count ranges
