// Error: calling mutating update on immutable BiasOverflowGuard
fn main() -> i32 {
    let guard = BiasOverflowGuard::new(32767);
    guard.update(100);
    0
}
