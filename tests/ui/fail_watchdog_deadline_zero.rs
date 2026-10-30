// Error: task deadline cannot be zero or negative
fn main() -> i32 {
    let mut wd = WatchdogMonitor::new();
    wd.register_task(0, 0);
    0
}
