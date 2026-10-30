// Error: calling mutating strobe on immutable WatchdogMonitor
fn main() -> i32 {
    let wd = WatchdogMonitor::new();
    wd.strobe(0);
    0
}
