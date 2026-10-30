// Actuator configuration with non-positive slew rate limit
fn main() -> i32 {
    let mut actuator = SlewRateLimiter::new(0, -500, -10000, 10000);
    0
}
