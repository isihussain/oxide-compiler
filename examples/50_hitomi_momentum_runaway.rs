// Example 50: JAXA Hitomi (ASTRO-H) Satellite Momentum Dump Runaway and Thruster Rate Interlock
// Mitigates satellite structural breakup from runaway thruster momentum dumps during excessive vehicle body rates.
// Inspired by spacecraft attitude fault management in RT-Thread (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize MomentumWheelGuard:
    // Max reaction wheel momentum = 1000 Nms, Max permissible body rate = 50 cdps (0.50 deg/s)
    let mut guard = MomentumWheelGuard::new(1000, 50);
    assert!(!guard.is_momentum_saturated());
    assert!(!guard.is_dump_allowed());
    assert!(!guard.is_runaway_tripped());
    assert!(guard.current_wheel_momentum_nms() == 0);
    assert!(guard.current_body_rate_cdps() == 0);
    assert!(guard.dump_burn_count() == 0);

    // 2. Nominal science observation mode:
    // Wheel momentum = 450 Nms (< 1000), Body rate = 4 cdps (< 50), No dump requested
    let ok1 = guard.update(450, 4, false);
    assert!(ok1);
    assert!(!guard.is_momentum_saturated());
    assert!(!guard.is_dump_allowed());
    assert!(!guard.is_runaway_tripped());
    assert!(guard.current_wheel_momentum_nms() == 450);

    // 3. Nominal reaction wheel desaturation dump:
    // Wheel momentum reaches 1020 Nms (>= 1000), Body rate is quiet at 8 cdps (< 50), Dump commanded
    let ok2 = guard.update(1020, 8, true);
    assert!(ok2);
    assert!(guard.is_momentum_saturated());
    assert!(guard.is_dump_allowed());
    assert!(!guard.is_runaway_tripped());
    assert!(guard.dump_burn_count() == 1);

    // 4. JAXA Hitomi thruster sign inversion and spin runaway scenario:
    // An erroneous thruster firing matrix accelerates vehicle body rate to 78 cdps (> 50 cdps).
    // Reaction wheels remain saturated at 1150 Nms, guidance commands another dump burn:
    let ok3 = guard.update(1150, 78, true);
    // Excessive body rate detected, runaway interlock trips and disables thrusters:
    assert!(!ok3);
    assert!(guard.is_momentum_saturated());
    assert!(guard.is_runaway_tripped());
    assert!(!guard.is_dump_allowed());
    // Burn count does not increment when locked out:
    assert!(guard.dump_burn_count() == 1);

    // 5. Secondary command rejection while interlock is active:
    let ok4 = guard.update(1200, 92, true);
    assert!(!ok4);
    assert!(guard.is_runaway_tripped());
    assert!(!guard.is_dump_allowed());

    // 6. Reset verification:
    let ok_reset = guard.reset();
    assert!(ok_reset);
    assert!(!guard.is_momentum_saturated());
    assert!(!guard.is_dump_allowed());
    assert!(!guard.is_runaway_tripped());
    assert!(guard.current_wheel_momentum_nms() == 0);
    assert!(guard.dump_burn_count() == 0);

    println!("JAXA Hitomi momentum runaway interlock verified successfully.");
    42
}
