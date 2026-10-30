// Actuator configuration with initial position exceeding physical stops
fn main() -> i32 {
    let mut actuator = SlewRateLimiter::new(15000, 5000, -10000, 10000);
    0
}
