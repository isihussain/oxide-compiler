// Example 52: ISRO Chandrayaan-2 Vikram Lander Descent Throttle Corridor Guard
// Prevents combustion flameout and excessive slew rates during closed-loop lunar descent braking.
// Inspired by thruster throttling management in RT-Thread (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize ThrustThrottleCorridor:
    // Min throttle = 40% (flameout margin), Max throttle = 100%, Max slew rate = 20% / second
    let mut corridor = ThrustThrottleCorridor::new(40, 100, 20);
    assert!(corridor.current_throttle() == 40);
    assert!(!corridor.is_clamped());
    assert!(!corridor.is_flameout_risk());
    assert!(corridor.clamped_count() == 0);

    // 2. Nominal throttling during rough braking phase:
    // Request 50% with dt = 1000 ms (permissible change = 20%)
    let t1 = corridor.command_throttle(50, 1000);
    assert!(t1 == 50);
    assert!(corridor.current_throttle() == 50);
    assert!(!corridor.is_clamped());
    assert!(!corridor.is_flameout_risk());
    assert!(corridor.clamped_count() == 0);

    // 3. Vikram lunar descent anomaly condition:
    // Trajectory error accumulation commands deep throttle reduction to 25% (below 40% flameout threshold)
    // with dt = 500 ms (slew limit = 10%)
    let t2 = corridor.command_throttle(25, 500);
    // Delta (-25%) exceeds slew limit (-10%), target clamped to 40% and flagged as flameout risk:
    assert!(t2 == 40);
    assert!(corridor.current_throttle() == 40);
    assert!(corridor.is_clamped());
    assert!(corridor.is_flameout_risk());
    assert!(corridor.clamped_count() == 1);

    // 4. Repeated sub-minimum command rejection:
    // Command 15% throttle with dt = 1000 ms
    let t3 = corridor.command_throttle(15, 1000);
    assert!(t3 == 40);
    assert!(corridor.is_clamped());
    assert!(corridor.is_flameout_risk());
    assert!(corridor.clamped_count() == 2);

    // 5. Emergency deceleration command requiring slew rate limiting:
    // Request 90% throttle from 40% baseline with dt = 1000 ms (max delta = +20%)
    let t4 = corridor.command_throttle(90, 1000);
    assert!(t4 == 60);
    assert!(corridor.current_throttle() == 60);
    assert!(corridor.is_clamped());
    assert!(!corridor.is_flameout_risk());
    assert!(corridor.clamped_count() == 3);

    // 6. Maximum limit clamping:
    // Command 120% throttle with dt = 2500 ms (max delta = +50%)
    let t5 = corridor.command_throttle(120, 2500);
    assert!(t5 == 100);
    assert!(corridor.current_throttle() == 100);
    assert!(corridor.is_clamped());
    assert!(corridor.clamped_count() == 4);

    // 7. Reset verification:
    let reset_ok = corridor.reset();
    assert!(reset_ok);
    assert!(corridor.current_throttle() == 40);
    assert!(!corridor.is_clamped());
    assert!(!corridor.is_flameout_risk());
    assert!(corridor.clamped_count() == 0);

    42
}
