// Error: servo_hard_stop_mrad must exceed max_disparity_mrad
fn main() -> i32 {
    let mut guard = InertialDisparityGuard::new(100, 50, 3);
    0
}
