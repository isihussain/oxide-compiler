// Example 26: Actuator Slew-Rate Limiter & TVC Gimbal Boundary Enforcement
//
// Rocket flight software requires physical actuator rate-of-change limits to prevent
// hydraulic pump cavitation, structural load exceedances, and aerodynamic stall.
// This example verifies static parameter validation and deterministic runtime clamping
// against maximum angular velocity and hard mechanical gimbal stops.

fn main() -> i32 {
    // Thrust Vector Control (TVC) pitch actuator:
    // Initial position: 0 mdeg (centerline)
    // Slew rate limit: 5000 mdeg/sec (5.0 deg/sec)
    // Physical gimbal mechanical stops: [-10000 mdeg, +10000 mdeg] (+/- 10 deg)
    let mut pitch_actuator = SlewRateLimiter::new(0, 5000, -10000, 10000);

    println!("TVC Actuator initialized at center: {} mdeg", pitch_actuator.current());

    // Step 1: Commanded target +20 deg (20000 mdeg) for dt = 100 ms
    // Rate limit allows max delta = (5000 * 100) / 1000 = 500 mdeg
    let p1 = pitch_actuator.step(20000, 100);
    println!("Step 1 position: {} mdeg", p1);
    if p1 != 500 {
        return 1;
    }
    if pitch_actuator.is_saturated() {
        return 2;
    }

    // Step 2: Commanded target +20 deg for dt = 2000 ms (2.0 s)
    // Slew rate allows max delta = 10000 mdeg -> target would reach 10500 mdeg
    // Physical mechanical stop clamps to max_bound: +10000 mdeg
    let p2 = pitch_actuator.step(20000, 2000);
    println!("Step 2 position (saturated at stop): {} mdeg", p2);
    if p2 != 10000 {
        return 3;
    }
    if !pitch_actuator.is_saturated() {
        return 4;
    }

    // Step 3: Reverse command towards -15 deg (-15000 mdeg) for dt = 200 ms
    // Rate limit allows max delta = (5000 * 200) / 1000 = 1000 mdeg
    // New position = 10000 - 1000 = 9000 mdeg
    let p3 = pitch_actuator.step(-15000, 200);
    println!("Step 3 position: {} mdeg", p3);
    if p3 != 9000 {
        return 5;
    }
    if pitch_actuator.is_saturated() {
        return 6;
    }

    // Step 4: Rapid slew to negative stop dt = 4000 ms (4.0 s)
    // Rate delta = 20000 mdeg -> target reaches -11000 mdeg
    // Mechanical stop clamps to min_bound: -10000 mdeg
    let p4 = pitch_actuator.step(-15000, 4000);
    println!("Step 4 position (saturated at negative stop): {} mdeg", p4);
    if p4 != -10000 {
        return 7;
    }
    if !pitch_actuator.is_saturated() {
        return 8;
    }

    println!("Actuator slew-rate and TVC mechanical boundaries verified successfully.");
    42
}
