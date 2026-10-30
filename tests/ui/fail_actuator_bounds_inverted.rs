// Actuator configuration with inverted gimbal boundaries (min_bound >= max_bound)
fn main() -> i32 {
    let mut actuator = SlewRateLimiter::new(0, 5000, 10000, -10000);
    0
}
