// Example 33: Attitude Determination Unit Quaternion Normalization & Drift Guard (NASA ADCS)
// Demonstrates unit quaternion invariants (|q|^2 = 1.0), spacecraft attitude composition,
// gyroscopic integration drift detection, and deterministic renormalization (SOHO mitigation).

fn main() -> i32 {
    // 1. Initialize identity quaternion (nominal zero-rotation reference)
    let q_ident = Quaternion::identity();
    assert!(q_ident.w() == 1000);
    assert!(q_ident.x() == 0);
    assert!(q_ident.y() == 0);
    assert!(q_ident.z() == 0);
    assert!(q_ident.norm_squared() == 1000000);
    assert!(q_ident.is_normalized());
    assert!(!q_ident.is_drifted());

    // 2. Initialize 90-degree pitch attitude rotation (cos(45 deg) = 0.7071)
    let q_pitch = Quaternion::new(707, 707, 0, 0);
    assert!(q_pitch.is_normalized());
    assert!(!q_pitch.is_drifted());

    // 3. Compose orientation via Hamilton quaternion product
    let q_rot = q_ident.multiply(q_pitch);
    assert!(q_rot.w() == 707);
    assert!(q_rot.x() == 707);
    assert!(q_rot.is_normalized());

    // 4. Quaternion conjugate for inverse orientation
    let q_inv = q_pitch.conjugate();
    assert!(q_inv.w() == 707);
    assert!(q_inv.x() == -707);

    // 5. Gyroscopic integration drift simulation (SOHO loss-of-attitude root cause)
    let mut q_tracker = Quaternion::new(707, 707, 0, 0);
    q_tracker.inject_drift(60, 50, 40, 30);
    assert!(q_tracker.is_drifted());
    assert!(!q_tracker.is_normalized());

    // 6. Execute deterministic drift guard and renormalize
    q_tracker.renormalize();
    assert!(q_tracker.is_normalized());
    assert!(!q_tracker.is_drifted());

    println!("NASA ADCS unit quaternion verification successful: norm = {}", q_tracker.norm_squared());
    42
}
