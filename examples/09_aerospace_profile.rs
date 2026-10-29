// Aerospace profile example: deterministic arithmetic without unbounded recursion
fn guidance_thrust(throttle: i32, multiplier: i32) -> i32 {
    let raw = throttle * multiplier;
    raw / 10
}

fn main() -> i32 {
    // 42 * 10 / 10 = 42
    let thrust = guidance_thrust(42, 10);
    thrust
}
