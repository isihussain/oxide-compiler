// Example 35: Watchdog Heartbeat Strobe Monitor & Task Deadline Failover (NASA JPL F Prime)
// Demonstrates real-time task heartbeat monitoring, missing strobe deadline detection,
// autonomous watchdog tripping, and recovery / failover to safe-mode.

fn main() -> i32 {
    let mut wd = WatchdogMonitor::new();

    // 1. Register flight tasks with deadline budgets (0: GNC 10 ticks, 1: ADCS 20 ticks, 2: Telemetry 50 ticks)
    assert!(wd.register_task(0, 10));
    assert!(wd.register_task(1, 20));
    assert!(wd.register_task(2, 50));
    assert!(!wd.is_tripped());

    // 2. Normal execution cycle: advance 5 ticks, tasks strobe keep-alive
    let ok1 = wd.tick(5);
    assert!(ok1);
    assert!(wd.is_task_healthy(0));
    assert!(wd.is_task_healthy(1));

    assert!(wd.strobe(0));
    assert!(wd.strobe(1));
    assert!(wd.strobe_count(0) == 1);
    assert!(wd.strobe_count(1) == 1);

    // 3. Advance 8 ticks: all tasks remain within deadline
    let ok2 = wd.tick(8);
    assert!(ok2);
    assert!(!wd.is_tripped());

    // 4. Strobe task 1 and 2, but simulate hung GNC task 0 (sensor bus lockup)
    assert!(wd.strobe(1));
    assert!(wd.strobe(2));

    // 5. Advance 5 ticks: Task 0 elapsed = 8 + 5 = 13 ticks > 10 tick deadline
    let ok3 = wd.tick(5);
    assert!(!ok3); // Watchdog tripped!
    assert!(wd.is_tripped());
    assert!(wd.tripped_task() == 0);
    assert!(!wd.is_task_healthy(0));
    assert!(wd.is_task_healthy(1));

    // 6. Execute safe-mode failover recovery and reset trip
    wd.reset_trip();
    assert!(!wd.is_tripped());
    assert!(wd.tripped_task() == -1);
    assert!(wd.is_task_healthy(0));

    println!("NASA JPL F Prime Watchdog verification successful: task deadline failover verified");
    42
}
