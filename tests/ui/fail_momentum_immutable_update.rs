// Error: calling mutating update on immutable MomentumWheelGuard
fn main() -> i32 {
    let guard = MomentumWheelGuard::new(1000, 50);
    guard.update(500, 10, false);
    0
}
