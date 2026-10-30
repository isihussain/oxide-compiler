// Error: task ID exceeds maximum monitored tasks [0..3]
fn main() -> i32 {
    let mut wd = WatchdogMonitor::new();
    wd.register_task(5, 10);
    0
}
