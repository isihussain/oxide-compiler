// Example 39: Cross-Channel Inertial Measurement Unit Disparity and Servo Runaway Guard
// Mitigates inertial guidance drift and servo runaway (Long March 3B / Intelsat 708 incident mitigation).
// Inspired by RT-Thread RTOS dual-redundant sensor parity arbitration (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize InertialDisparityGuard:
    // max_disparity = 200 cdeg (2.0 deg), max_servo_rate = 500 cdeg/s, disparity_limit = 3 frames
    let mut imu_guard = InertialDisparityGuard::new(200, 500, 3);
    assert!(imu_guard.active_channel() == 0); // Primary channel active
    assert!(!imu_guard.is_backup_engaged());
    assert!(!imu_guard.is_servo_runaway());
    assert!(imu_guard.disparity_count() == 0);

    // 2. Nominal tracking: Primary = 150 cdeg, Backup = 152 cdeg (diff = 2 <= 200), servo_rate = 120 cdeg/s
    let out1 = imu_guard.update(150, 152, 120);
    assert!(out1 == 150); // Primary angle selected
    assert!(imu_guard.active_channel() == 0);
    assert!(imu_guard.disparity_count() == 0);

    // 3. Transient noise glitch: Primary spikes to 370 cdeg, backup = 152 cdeg (diff = 218 > 200)
    let out2 = imu_guard.update(370, 152, 130);
    assert!(out2 == 370); // Glitch tolerated on frame 1
    assert!(imu_guard.disparity_count() == 1);
    assert!(imu_guard.active_channel() == 0);

    // 4. Glitch recovery: Primary returns to 154 cdeg, backup = 153 cdeg
    let out3 = imu_guard.update(154, 153, 110);
    assert!(out3 == 154);
    assert!(imu_guard.disparity_count() == 0);
    assert!(imu_guard.active_channel() == 0);

    // 5. Persistent gyro divergence (Intelsat 708 short-circuit failure scenario):
    // Primary inertial platform drifts away rapidly: 500, 750, 1100 cdeg
    // Backup strapdown IMU maintains true trajectory: 156, 158, 160 cdeg
    let d1 = imu_guard.update(500, 156, 140);
    assert!(d1 == 500);
    assert!(imu_guard.disparity_count() == 1);

    let d2 = imu_guard.update(750, 158, 145);
    assert!(d2 == 750);
    assert!(imu_guard.disparity_count() == 2);

    let d3 = imu_guard.update(1100, 160, 150);
    // 3 consecutive disparity frames: Primary channel isolated, backup engaged!
    assert!(d3 == 160);
    assert!(imu_guard.active_channel() == 1);
    assert!(imu_guard.is_backup_engaged());
    assert!(imu_guard.selected_angle() == 160);

    // 6. Channel lockout: Subsequent frames preserve backup channel
    let d4 = imu_guard.update(1500, 165, 120);
    assert!(d4 == 165);
    assert!(imu_guard.active_channel() == 1);

    // 7. Servo runaway immediate trigger test:
    // When servo feedback loop opens, commanded rate exceeds structural limits (650 cdeg/s > 500)
    let mut runaway_guard = InertialDisparityGuard::new(200, 500, 3);
    let r1 = runaway_guard.update(100, 102, 650);
    assert!(r1 == 102); // Swapped immediately to backup
    assert!(runaway_guard.is_servo_runaway());
    assert!(runaway_guard.is_backup_engaged());
    assert!(runaway_guard.active_channel() == 1);

    println!("Long March 3B inertial disparity and servo runaway guard verified successfully.");
    42
}
