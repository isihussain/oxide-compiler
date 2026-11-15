// Example 49: ExoMars Schiaparelli Altimeter Saturation and Doppler-Kalman Consistency Gate
// Mitigates premature parachute jettison and free-fall impact from corrupt negative altitude integration.
// Inspired by fault-tolerant navigation and sensor gating patterns in RT-Thread (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize DescentKalmanGate:
    // Max allowable gyro angular rate = 250 deg/s, Max altitude divergence = 100 m
    let mut gate = DescentKalmanGate::new(250, 100);
    assert!(!gate.is_imu_saturated());
    assert!(!gate.is_divergence_detected());
    assert!(!gate.is_jettison_blocked());
    assert!(gate.current_altitude_m() == 100000);
    assert!(gate.valid_sample_count() == 0);

    // 2. Nominal parachute descent phase at 4000 m:
    // IMU rate = 25 dps (well within bounds), radar altimeter = 4000 m, dead-reckoning = 3980 m (diff = 20 <= 100)
    let ok1 = gate.update(25, 4000, 3980);
    assert!(ok1);
    assert!(!gate.is_imu_saturated());
    assert!(!gate.is_divergence_detected());
    assert!(!gate.is_jettison_blocked());
    assert!(gate.current_altitude_m() == 3980);
    assert!(gate.valid_sample_count() == 1);

    // 3. ExoMars Schiaparelli parachute oscillation anomaly (T+242s):
    // Angular rate sensor saturates at 320 dps (> 250 dps).
    // Corrupted integrated attitude calculation flips coordinate frame, yielding negative altitude (-2 m)
    // while the Doppler radar altimeter reliably reads 3700 m.
    let ok2 = gate.update(320, 3700, -2);
    // Negative altitude divergence detected, premature parachute jettison strictly blocked!
    assert!(!ok2);
    assert!(gate.is_imu_saturated());
    assert!(gate.is_divergence_detected());
    assert!(gate.is_jettison_blocked());
    // Direct fallback to raw radar altitude preserves vehicle height state:
    assert!(gate.current_altitude_m() == 3700);
    assert!(gate.valid_sample_count() == 1);

    // 4. Continued divergence with excessive delta (radar = 3500 m, integrated = 3200 m, diff = 300 > 100):
    let ok3 = gate.update(180, 3500, 3200);
    assert!(!ok3);
    assert!(!gate.is_imu_saturated());
    assert!(gate.is_divergence_detected());
    assert!(gate.is_jettison_blocked());
    assert!(gate.current_altitude_m() == 3500);

    // 5. Angular rate stabilizes and filter re-converges at 2000 m:
    let ok4 = gate.update(40, 2000, 1980);
    assert!(ok4);
    assert!(!gate.is_imu_saturated());
    assert!(!gate.is_divergence_detected());
    assert!(!gate.is_jettison_blocked());
    assert!(gate.current_altitude_m() == 1980);
    assert!(gate.valid_sample_count() == 2);

    // 6. Reset verification:
    let ok_reset = gate.reset();
    assert!(ok_reset);
    assert!(!gate.is_imu_saturated());
    assert!(!gate.is_divergence_detected());
    assert!(!gate.is_jettison_blocked());
    assert!(gate.current_altitude_m() == 100000);
    assert!(gate.valid_sample_count() == 0);

    println!("ExoMars Schiaparelli descent Kalman gating verified successfully.");
    42
}
