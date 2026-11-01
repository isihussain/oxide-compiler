// Error: calling mutating update on immutable ControlSurfaceGuard
fn main() -> i32 {
    let guard = ControlSurfaceGuard::new(4, 150, 3);
    guard.update(0, 500, 500);
    0
}
