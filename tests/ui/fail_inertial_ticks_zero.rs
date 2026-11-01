// Error: max_disparity_ticks must be positive
fn main() -> i32 {
    let mut guard = InertialDisparityGuard::new(50, 150, 0);
    0
}
