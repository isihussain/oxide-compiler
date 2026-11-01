// Example 40: Multi-Engine Cluster Differential Gimbal Trim and Burn Reshaping
// Mitigates engine-out attitude divergence and orbit insertion shortfall (LandSpace Zhuque-2 incident mitigation).
// Inspired by RT-Thread RTOS mission fault accommodation routines (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize EngineOutCompensator:
    // 4-engine vernier cluster, 20 kN thrust per engine, 3000 m/s delta-v, 150 seconds planned duration
    let mut comp = EngineOutCompensator::new(4, 20, 3000, 150);
    assert!(comp.active_engines() == 4);
    assert!(!comp.is_engine_out());
    assert!(!comp.is_burn_completed());
    assert!(comp.failed_engine_id() == -1);
    assert!(comp.trim_gimbal_cdeg() == 0);
    assert!(comp.reshaped_burn_duration() == 150);

    // 2. Nominal firing phase (T+40s):
    // All 4 engines firing nominally: mask = 0b1111 = 15
    let cont1 = comp.update(15, 40);
    assert!(cont1);
    assert!(!comp.is_engine_out());
    assert!(comp.trim_gimbal_cdeg() == 0);
    assert!(comp.reshaped_burn_duration() == 150);

    // 3. Vernier engine 2 premature shutdown at T+60s (Zhuque-2 maiden flight scenario):
    // Engine 2 flames out: mask = 0b1011 = 11 (engines 0, 1, 3 firing; engine 2 failed)
    let cont2 = comp.update(11, 60);
    assert!(cont2);
    assert!(comp.is_engine_out());
    assert!(comp.active_engines() == 3);
    assert!(comp.failed_engine_id() == 2);

    // Dynamic gimbal counter-torque trims opposite actuators to null asymmetric moment:
    // Commanded trim = 150 cdeg (1.5 degrees)
    assert!(comp.trim_gimbal_cdeg() == 150);

    // Burn duration dynamically reshaped:
    // Remaining planned burn at T+60s was 90s.
    // With 3 active engines out of 4, remaining duration scales by 4/3: (90 * 4) / 3 = 120s.
    // New burn cutoff time = 60 + 120 = 180s.
    assert!(comp.reshaped_burn_duration() == 180);

    // 4. Continued burn at T+120s: trim and reshaped duration remain active
    let cont3 = comp.update(11, 120);
    assert!(cont3);
    assert!(!comp.is_burn_completed());
    assert!(comp.trim_gimbal_cdeg() == 150);
    assert!(comp.reshaped_burn_duration() == 180);

    // 5. Burn completion at T+180s:
    // Compensated burn window expires, target orbital velocity achieved
    let cont4 = comp.update(11, 180);
    assert!(!cont4); // Burn completed
    assert!(comp.is_burn_completed());

    println!("LandSpace Zhuque-2 engine-out trim and burn reshaping verified successfully.");
    42
}
