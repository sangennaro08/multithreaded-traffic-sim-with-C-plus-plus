[← Back to main README](../MAIN_README.md)

# 🧠 `Traffic_algorithm` — Direction Assignment & Emergency Rush

The traffic algorithm is responsible for assigning each vehicle a directional slot in the **next** intersection before it arrives there. This pre-assignment prevents deadlocks and contention from all vehicles competing for the same slot simultaneously.

---

## `start_algorithm`

Entry point called by each vehicle thread during `simulate_crossing`. Runs in a `do-while` loop until `V->choosed` is `true`.

Each iteration:

1. Acquires `find_pattern` mutex on the current intersection
2. If `can_decide` is `true` and the vehicle has not been assigned yet:
   - Sets `can_decide = false`
   - Calls `search_exit` (batch assignment)
   - Restores `can_decide = true`
   - Notifies `get_direction_cv`
3. Otherwise: waits on `get_direction_cv` until `V->choosed` is `true`
4. Increments `attempts` and calls `go_alone`

The `can_decide` flag ensures only one thread runs `search_exit` at a time per intersection, avoiding redundant batch assignment passes.

---

## `search_exit` — Batch Direction Assignment

Acquires `direction_mutex` on **both** the current and next intersection simultaneously.

**Assignment logic:**

```
to_move = min(next->free_spots, current->vehicles_in_intersection.size())
```

For each of the first `to_move` vehicles in `vehicles_in_intersection`:
- Iterates `next->directions`
- Assigns the first available slot that is **not** `"Forward"` and **not already assigned** to this vehicle
- Sets `choosed = true` on each assigned vehicle

This assigns directions to **multiple vehicles at once** in a single critical section, minimizing contention. Acquiring `direction_mutex` on both intersections simultaneously via `scoped_lock` prevents deadlocks regardless of acquisition order.

---

## `go_alone` — Starvation Fallback

If `attempts >= 3` and the vehicle still has no direction assigned:

1. Acquires `direction_mutex` on the next intersection
2. Picks a **random** direction from the map (excluding `"Forward"`)
3. Assigns it **regardless of availability**
4. Sets `choosed = true`

This is a fallback to prevent starvation when `search_exit` cannot find a free slot. The direction may be occupied — the vehicle will then wait in the appropriate `entry_queue` until it becomes free.

---

## Emergency Rush (`rush_to_hospital`)

Triggered from `simulate_crossing` when `run()` returns `true` — i.e., the ambulance has exceeded its time threshold (`120 / patients` seconds).

While `idx != last_intersection`:

1. Calls `set_directions` on current intersection (acquires direction slot normally)
2. Sleeps `crossing_time / 2` seconds (half normal crossing time)
3. Calls `leave_intersection` on current intersection
4. Sleeps `arrival_time / 2` seconds (half normal travel time)
5. Pushes **directly** into `next->vehicles_in_intersection` under `direction_mutex` — **no semaphore, no queue, no `free_spots` check**
6. Updates `idx`

The ambulance bypasses the semaphore and queuing entirely, simulating a real emergency vehicle overriding traffic. The `direction_mutex` is still acquired when pushing into `vehicles_in_intersection` because other threads may be reading or modifying that vector concurrently.

---

## Design Decisions

### Why exclude `"Forward"` from direction assignments?

`"Forward"` means the vehicle exits toward the next intersection in the chain. It is the implicit movement direction and does not need to be explicitly allocated as a slot. Both `search_exit` and `go_alone` enforce `dir != "Forward"` when scanning for available slots.

### Why batch assignment in `search_exit`?

Without batching, every vehicle would call into `start_algorithm` independently, each acquiring the mutex separately. Batching assigns directions to all vehicles currently waiting in one pass — reducing total lock acquisitions by a factor proportional to intersection occupancy.

### `scoped_lock` with two mutexes in `search_exit`

`search_exit` reads `free_spots` (needs `direction_mutex` on next) and reads `vehicles_in_intersection` (needs `direction_mutex` on current). Acquiring them separately would risk deadlock if another thread holds one and waits for the other. `std::scoped_lock` acquires both atomically using a deadlock-avoidance algorithm, making acquisition order irrelevant.

### `attempts` counter

Each iteration of the `do-while` loop in `start_algorithm` increments `attempts`. After 3 failed attempts (vehicle still not assigned by `search_exit`), `go_alone` kicks in. This threshold balances giving `search_exit` a fair chance to batch-assign vs. not starving any single vehicle indefinitely.
