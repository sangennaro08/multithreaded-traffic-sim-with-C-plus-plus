[← Back to main README](../README.md)

# 🏛️ `Intersection` — Shared Resource

Each `Intersection` object represents one node in the chain that all vehicles must cross sequentially. It is the central shared resource of the simulation and holds all the synchronization primitives needed to coordinate concurrent access.

---

## Fields

| Field | Type | Purpose |
|---|---|---|
| `enter` | `counting_semaphore<3>` | Hard limit: at most `max_per_intersection` (3) vehicles inside at once |
| `free_spots` | `int` | Mirror of available capacity, updated under `direction_mutex` |
| `directions` | `unordered_map<string, bool>` | 4 directional slots: `"Right"`, `"Left"`, `"Back"`, `"Forward"`. `true` = available |
| `entry_queues` | `unordered_map<string, queue<Vehicle*>>` | Per-direction waiting queues for `"Right"`, `"Left"`, `"Back"` |
| `vehicles_in_intersection` | `vector<Vehicle*>` | Vehicles currently inside, used by the traffic algorithm |
| `direction_mutex` | `mutex` | Protects `directions`, `free_spots`, and `vehicles_in_intersection` |
| `find_pattern` | `mutex` | Serializes direction assignment in the traffic algorithm |
| `get_direction_cv` | `condition_variable` | Notifies waiting vehicles when a direction has been assigned |
| `exit_cv` | `condition_variable` | Notified when a vehicle leaves, waking queued vehicles |

---

## Why No Queue for `"Forward"`?

`entry_queues` only contains `"Right"`, `"Left"`, and `"Back"`. The `"Forward"` direction is excluded because a vehicle moving forward is already heading to the **next intersection in the chain** — it does not need to queue at the current one. The traffic algorithm also excludes `"Forward"` from all direction assignments.

---

## `wait_in_queue`

Called for every intersection that is not the final one. Acquires `direction_mutex` on the next intersection. If the requested `entry_direction` slot is already occupied:

1. Pushes the vehicle into the corresponding `entry_queues` queue
2. Calls `add_queue()` to increment the car's `queues_entered` counter
3. Waits on `exit_cv` until **two conditions** are simultaneously true:
   - The vehicle is at the **front** of its queue
   - The requested direction slot is **free**
4. Pops itself from the queue

After waiting (or immediately if the slot was already free), the vehicle pushes itself into `vehicles_in_intersection` and marks its direction as occupied.

---

## `leave_intersection`

Acquires `direction_mutex`. Removes the vehicle from `vehicles_in_intersection`, marks `exit_direction` as available again (`true`), then calls `exit_cv.notify_all()` to wake all vehicles waiting in queues on this intersection.

---

## `set_directions`

Called under `direction_mutex` at the start of crossing each intersection.

- If `entry_direction` is **empty** (first intersection, or car with no pre-assigned direction): iterates `directions` and takes the **first available** slot, sets `exit_direction` to it and marks it `false`.
- If `entry_direction` is **already set** (assigned by the traffic algorithm ahead of time): copies it to `exit_direction` and clears `entry_direction`.

---

## Initialization

### `initialize_intersections`
Creates `total_intersections` `Intersection` objects stored as `unique_ptr` in a vector.

### `initialize_cars`
Creates `total_cars` `Car` objects. Each car is assigned a starting intersection via `pick_intersection`, which randomly samples intersections until one with fewer than 3 vehicles is found. The car is immediately pushed into that intersection's `vehicles_in_intersection`.

### `create_ambulances`
Creates between 1 and `(total_cars / 8) + 1` `Ambulance` objects. They are stored in the ambulances vector but **not launched** — they wait until the simulation triggers them via `launch_ambulance`. The number of ambulances scales with the car count.

### `launch_cars`
Starts one `std::thread` per car, each running `simulate_crossing` starting from the car's `start_intersection`.

---

## Semaphore vs `free_spots`

The semaphore `enter` enforces the hard cap of 3 vehicles. `free_spots` is an `int` that mirrors the same count but is readable under `direction_mutex` — it is used by the traffic algorithm (`search_exit`) to know how many vehicles it can assign directions to without acquiring the semaphore. They are always kept in sync.

`try_acquire` is used instead of `acquire` to avoid blocking the thread indefinitely in the main loop. In practice the semaphore never blocks because `pick_intersection` ensures at most 3 cars start at any intersection.
