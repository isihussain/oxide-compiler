// Error: calling mutating update on immutable InertialDisparityGuard
fn main() -> i32 {
    let guard = InertialDisparityGuard::new(50, 150, 3);
    guard.update(10, 10, 0);
    0
}
