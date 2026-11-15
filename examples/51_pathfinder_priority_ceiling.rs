// Example 51: NASA Mars Pathfinder Priority Ceiling Protocol (PCP) Mutex
// Prevents real-time priority inversion between attitude control, communications, and ASI/MET threads.
// Inspired by real-time mutex priority inheritance protocols in RT-Thread (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize PriorityCeilingMutex with ceiling priority 30 (highest task priority on the bus):
    let mut mutex = PriorityCeilingMutex::new(30);
    assert!(!mutex.is_locked());
    assert!(mutex.owner_task_id() == -1);
    assert!(mutex.boosted_priority() == 0);
    assert!(mutex.inversion_prevented_count() == 0);

    // 2. Low-priority task: Meteorological / Attitude Sensing (ASI/MET), task 1, base priority 10
    // Task 1 acquires the information bus mutex:
    let locked_by_task1 = mutex.lock(1, 10);
    assert!(locked_by_task1);
    assert!(mutex.is_locked());
    assert!(mutex.owner_task_id() == 1);
    // Task 1 inherits ceiling priority to prevent preemption by medium priority tasks:
    assert!(mutex.boosted_priority() == 30);

    // 3. Medium-priority task: Communications task, task 2, base priority 20
    // In unmitigated Mars Pathfinder runtime, task 2 preempted task 1 indefinitely.
    // Under Priority Ceiling Protocol, task 1 runs at ceiling 30, suppressing preemption by task 2.

    // 4. High-priority task: Attitude control thread (bc_dist), task 3, base priority 30
    // Task 3 requests the mutex while held by task 1:
    let locked_by_task3 = mutex.lock(3, 30);
    assert!(!locked_by_task3);
    assert!(mutex.is_locked());
    assert!(mutex.owner_task_id() == 1);
    // Mutex registers priority contention and increments prevention counter:
    assert!(mutex.inversion_prevented_count() == 1);
    assert!(mutex.boosted_priority() == 30);

    // 5. Task 1 finishes critical section and releases mutex:
    let unlocked_task1 = mutex.unlock(1);
    assert!(unlocked_task1);
    assert!(!mutex.is_locked());
    assert!(mutex.owner_task_id() == -1);
    assert!(mutex.boosted_priority() == 0);

    // 6. High-priority task 3 now successfully acquires the mutex:
    let locked_now_task3 = mutex.lock(3, 30);
    assert!(locked_now_task3);
    assert!(mutex.is_locked());
    assert!(mutex.owner_task_id() == 3);
    assert!(mutex.boosted_priority() == 30);

    // 7. Task 3 finishes attitude telemetry publish and releases mutex:
    let unlocked_task3 = mutex.unlock(3);
    assert!(unlocked_task3);
    assert!(!mutex.is_locked());
    assert!(mutex.owner_task_id() == -1);

    // 8. Re-lock and reset verification:
    let relock = mutex.lock(1, 10);
    assert!(relock);
    assert!(mutex.is_locked());
    let reset_ok = mutex.reset();
    assert!(reset_ok);
    assert!(!mutex.is_locked());
    assert!(mutex.inversion_prevented_count() == 0);

    42
}
