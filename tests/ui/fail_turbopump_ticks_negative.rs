// Error: max_cavitation_ticks must be positive in TurbopumpMonitor
fn main() -> i32 {
    let mut tp = TurbopumpMonitor::new(35000, 5000, -1);
    0
}
