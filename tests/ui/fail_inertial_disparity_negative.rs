// Error: max_disparity_mrad must be positive
fn main() -> i32 {
    let mut guard = InertialDisparityGuard::new(-50, 150, 3);
    0
}
