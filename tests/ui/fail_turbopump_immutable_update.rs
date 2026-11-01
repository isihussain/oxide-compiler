// Error: calling mutating update on immutable TurbopumpMonitor
fn main() -> i32 {
    let tp = TurbopumpMonitor::new(10000, 25000, 1500, 3);
    tp.update(18000, 300, 2500);
    0
}
