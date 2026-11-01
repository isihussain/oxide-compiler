// Error: max_rpm must be positive in TurbopumpMonitor
fn main() -> i32 {
    let mut tp = TurbopumpMonitor::new(0, 5000, 3);
    0
}
