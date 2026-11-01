// Error: calling mutating update on immutable TurbopumpMonitor
fn main() -> i32 {
    let tp = TurbopumpMonitor::new(35000, 5000, 3);
    tp.update(28000, 6000);
    0
}
