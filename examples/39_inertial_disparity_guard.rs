// Example 39: Cross-Channel Inertial Measurement Unit Disparity and Servo Runaway Guard
// Mitigates uncontrolled pitch-over and guidance short circuits (Long March 3B / Intelsat 708 incident mitigation).
// Enforces cross-channel disparity bounds and detects TVC servo runaway toward mechanical hard stops.

fn main() -> i32 {
    // 1. Initialize InertialDisparityGuard:
    // max_disparity_mrad = 50 mrad (~2.86 degrees)
    // servo_hard_stop_mrad = 150 mrad (~8.59 degrees)
    // max_disparity_ticks = 3 consecutive frames
    let mut guard = InertialDisparityGuard::new(50, 150, 3);
    assert!(guard.is_nominal());
    assert!(guard.is_primary_active());
    assert!(guard.active_channel() == 0);
    assert!(guard.disparity_ticks() == 0);

    // 2. Nominal atmospheric ascent: Primary = 120 mrad, Secondary = 122 mrad (diff = 2 mrad), Servo = 15 mrad
    let s1 = guard.update(120, 122, 15);
    let s2 = guard.update(130, 128, 18);
    assert!(s1 == 0);
    assert!(s2 == 0);
    assert!(guard.is_nominal());
    assert!(guard.last_disparity() == 2);
    assert!(guard.is_primary_active());

    // 3. Transient disparity noise (e.g. electrical glitch on primary platform):
    // Primary = 180 mrad, Secondary = 120 mrad (diff = 60 mrad > 50 mrad limit)
    // Frame 1 enters Warning state (1), verifying persistence
    let s3 = guard.update(180, 120, 20);
    assert!(s3 == 1);
    assert!(guard.is_warning());
    assert!(guard.disparity_ticks() == 1);
    assert!(guard.last_disparity() == 60);
    assert!(guard.is_primary_active());

    // 4. Transient recovery: Next frame diff returns to nominal (diff = 10 mrad)
    let s4 = guard.update(135, 125, 20);
    assert!(s4 == 0);
    assert!(guard.is_nominal());
    assert!(guard.disparity_ticks() == 0);

    // 5. Persistent guidance platform short circuit: 3 consecutive divergent frames
    let d1 = guard.update(200, 130, 30);
    assert!(d1 == 1);
    assert!(guard.is_warning());
    assert!(guard.disparity_ticks() == 1);

    let d2 = guard.update(210, 132, 35);
    assert!(d2 == 1);
    assert!(guard.is_warning());
    assert!(guard.disparity_ticks() == 2);

    let d3 = guard.update(220, 135, 40);
    assert!(d3 == 2);
    assert!(guard.is_isolated());
    assert!(guard.is_backup_engaged());
    assert!(guard.active_channel() == 1);
    assert!(guard.trip_code() == 1);

    // 6. Channel lockout latch: subsequent updates remain locked to backup reference
    let s_locked = guard.update(130, 130, 10);
    assert!(s_locked == 2);
    assert!(guard.is_isolated());
    assert!(guard.is_backup_engaged());

    // 7. Servo runaway toward mechanical hard stop test
    let mut guard2 = InertialDisparityGuard::new(50, 150, 3);
    assert!(guard2.is_nominal());
    let s_runaway = guard2.update(100, 102, 155);
    assert!(s_runaway == 2);
    assert!(guard2.is_isolated());
    assert!(guard2.is_backup_engaged());
    assert!(guard2.trip_code() == 2);

    println!("Inertial disparity and servo runaway guard verification successful");
    42
}
