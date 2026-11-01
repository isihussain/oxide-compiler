// Error: calling mutating update on immutable InertialDisparityGuard
fn main() -> i32 {
    let guard = InertialDisparityGuard::new(200, 500, 3);
    guard.update(100, 100, 50);
    0
}
