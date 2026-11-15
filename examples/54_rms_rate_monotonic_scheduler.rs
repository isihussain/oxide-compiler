// Example 54: Static Rate-Monotonic Scheduling (RMS) Schedulability Verifier
// Proves deterministic real-time schedulability of periodic avionics tasks under the Liu & Layland bound.
// Inspired by rate-monotonic real-time priority scheduling in RT-Thread (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize RmsScheduler with max capacity of 8 flight tasks:
    let mut scheduler = RmsScheduler::new(8);
    assert!(scheduler.task_count() == 0);
    assert!(scheduler.total_utilization_basis_points() == 0);
    assert!(scheduler.is_schedulable());
    assert!(!scheduler.is_overloaded());

    // 2. Register Task 1: Attitude and Rate Control loop (100 Hz, period 10 ms, WCET 2 ms):
    // Utilization: 2000 / 10000 = 20.00% (2000 bp). Bound for 1 task = 100.00% (10000 bp).
    let ok1 = scheduler.add_task(1, 10000, 2000);
    assert!(ok1);
    assert!(scheduler.task_count() == 1);
    assert!(scheduler.total_utilization_basis_points() == 2000);
    assert!(scheduler.assigned_priority(1) == 100);

    // 3. Register Task 2: Navigation and Kalman State Estimation (50 Hz, period 20 ms, WCET 4 ms):
    // Utilization: 4000 / 20000 = 20.00% (2000 bp). Cumulative = 4000 bp.
    // Liu & Layland bound for 2 tasks = 82.84% (8284 bp). 4000 <= 8284 -> Schedulable!
    let ok2 = scheduler.add_task(2, 20000, 4000);
    assert!(ok2);
    assert!(scheduler.task_count() == 2);
    assert!(scheduler.total_utilization_basis_points() == 4000);
    assert!(scheduler.assigned_priority(2) == 50);
    // Rate-Monotonic principle: higher frequency task 1 has higher priority than task 2:
    assert!(scheduler.assigned_priority(1) > scheduler.assigned_priority(2));

    // 4. Register Task 3: Guidance and Trajectory Shaping (20 Hz, period 50 ms, WCET 10 ms):
    // Utilization: 10000 / 50000 = 20.00% (2000 bp). Cumulative = 6000 bp.
    // Liu & Layland bound for 3 tasks = 77.97% (7797 bp). 6000 <= 7797 -> Schedulable!
    let ok3 = scheduler.add_task(3, 50000, 10000);
    assert!(ok3);
    assert!(scheduler.task_count() == 3);
    assert!(scheduler.total_utilization_basis_points() == 6000);
    assert!(scheduler.assigned_priority(3) == 20);

    // 5. Register Task 4: Telemetry Downlink and Health Housekeeping (10 Hz, period 100 ms, WCET 10 ms):
    // Utilization: 10000 / 100000 = 10.00% (1000 bp). Cumulative = 7000 bp.
    // Liu & Layland bound for 4 tasks = 75.68% (7568 bp). 7000 <= 7568 -> Schedulable!
    let ok4 = scheduler.add_task(4, 100000, 10000);
    assert!(ok4);
    assert!(scheduler.task_count() == 4);
    assert!(scheduler.total_utilization_basis_points() == 7000);
    assert!(scheduler.assigned_priority(4) == 10);
    assert!(scheduler.is_schedulable());
    assert!(!scheduler.is_overloaded());

    // 6. Schedulability overload detection:
    // Attempt to register Task 5 with excessive WCET (period 50 ms, WCET 20 ms, 40.00% / 4000 bp):
    // Cumulative would be 7000 + 4000 = 11000 bp (> 100.00%, exceeds 5-task bound of 7434 bp):
    let ok5 = scheduler.add_task(5, 50000, 20000);
    // Schedulability bound breached: task rejected and overload tripped:
    assert!(!ok5);
    assert!(scheduler.is_overloaded());
    // Task count and utilization remain uncorrupted:
    assert!(scheduler.task_count() == 4);
    assert!(scheduler.total_utilization_basis_points() == 7000);

    // 7. Reset verification:
    let reset_ok = scheduler.reset();
    assert!(reset_ok);
    assert!(scheduler.task_count() == 0);
    assert!(scheduler.total_utilization_basis_points() == 0);
    assert!(scheduler.is_schedulable());
    assert!(!scheduler.is_overloaded());

    42
}
