[← Back to main README](../MAIN_README.md)

# 🏛️ `Intersection` — Shared Resource

Each `Intersection` object represents one node in the chain that all vehicles must cross sequentially. It is the central shared resource of the simulation and holds all the synchronization primitives needed to coordinate concurrent access.

---

## Fields

| Field | Type | Purpose |
|---|---|---|
| `enter` | `counting_semaphore<3>` | Limits simultaneous vehicles to `max_per_intersection` |
| `free_spots` | `int` | Mirrors available capacity, updated under `direction_mutex` |
| `directions` | `unordered_map<string, bool>` | Four slots: `"Right"`, `"Left"`, `"Back"`, `"Forward"`. `true` = available, `false` = occupied |
| `entry_queues` | `unordered_map<string, queue<Vehicle*>>` | Per-direction waiting queues for `"Right"`, `"Left"`, `"Back"` |
| `vehicles_in_intersection` | `vector<Vehicle*>` | Pointers to vehicles currently inside |
| `direction_mutex` | `mutex` | Protects `directions`, `free_spots`, and `vehicles_in_intersection` |
| `find_pattern` | `mutex` | Used by the traffic algorithm to serialize direction assignment |
| `get_direction_cv` | `condition_variable` | Notifies waiting vehicles when a direction has been assigned |
| `exit_cv` | `condition_variable` | Notified when a vehicle leaves; used by queued vehicles to re-check their slot |

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

## `set_directions`

Called under `direction_mutex`.

- If `entry_direction` is **empty** (first intersection or car with no pre-assigned direction): iterates `directions` and takes the **first available** slot, sets `exit_direction` to it and marks it `false`.
- If `entry_direction` is **already set** (assigned by traffic algorithm): copies it to `exit_direction` and clears `entry_direction`.

---

## `wait_in_queue`

Called for every intersection that is not the final one.

Acquires `direction_mutex` on the next intersection. If the requested `entry_direction` slot is already occupied (`directions.at(entry_direction) == false`):

1. Pushes the vehicle into the corresponding `entry_queues` queue
2. Increments `queues_entered` via `add_queue()`
3. Waits on `exit_cv` until two conditions are simultaneously true:
   - The vehicle is at the **front** of its queue
   - The requested direction slot is **free**
4. Pops itself from the queue

After waiting (or immediately if the slot was free), pushes the vehicle into `vehicles_in_intersection` and marks the direction as occupied.

---

## `leave_intersection`

Acquires `direction_mutex`. Removes the vehicle from `vehicles_in_intersection`, marks `exit_direction` as available again (`true`), and calls `exit_cv.notify_all()` to wake all vehicles waiting in queues on this intersection.

---

## Design Decisions

### `"Forward"` has no entry queue

The `entry_queues` map only contains `"Right"`, `"Left"`, and `"Back"`. `"Forward"` is excluded because a vehicle moving forward is traveling to the next intersection in the chain — it does not need to queue at the current one. The traffic algorithm also excludes `"Forward"` from direction assignments (`dir != "Forward"` in `search_exit` and `go_alone`).

### Semaphore vs `free_spots`

The semaphore `enter` enforces the hard cap of 3 vehicles. `free_spots` is an `int` that mirrors the same count but is readable under `direction_mutex` — it is used by the traffic algorithm (`search_exit`) to know how many vehicles it can assign directions to in one pass. They are always kept in sync.

### `try_acquire` instead of `acquire` on the semaphore

`acquire()` would block the thread indefinitely. `try_acquire()` returns immediately if the semaphore count is zero, allowing the for loop to retry. In practice the semaphore never blocks because `pick_intersection` ensures at most 3 cars start at any intersection, and cars move through sequentially — but `try_acquire` is the correct non-blocking pattern for a loop that needs to keep re-evaluating its exit condition.
