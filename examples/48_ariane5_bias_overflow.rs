// Example 48: Ariane 501 Horizontal Bias Numerical Overflow Guard
// Mitigates SRI computer halt from unhandled 64-bit to 16-bit conversion overflow (Ariane 501 incident mitigation).
// Inspired by RT-Thread RTOS math conversion safety guards (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize BiasOverflowGuard:
    // Max allowable 16-bit signed bias integer value = 32767
    let mut guard = BiasOverflowGuard::new(32767);
    assert!(!guard.is_overflow_detected());
    assert!(!guard.is_backup_engaged());
    assert!(guard.current_bias_scaled() == 0);
    assert!(guard.valid_sample_count() == 0);

    // 2. Nominal alignment phase (T+0s to T+30s):
    let ok1 = guard.update(120);
    assert!(ok1);
    assert!(guard.current_bias_scaled() == 120);
    assert!(guard.valid_sample_count() == 1);

    let ok2 = guard.update(-450);
    assert!(ok2);
    assert!(guard.current_bias_scaled() == -450);
    assert!(guard.valid_sample_count() == 2);

    let ok3 = guard.update(31500);
    assert!(ok3);
    assert!(guard.current_bias_scaled() == 31500);
    assert!(guard.valid_sample_count() == 3);
    assert!(!guard.is_overflow_detected());
    assert!(!guard.is_backup_engaged());

    // 3. Ariane 501 high horizontal velocity anomaly (T+37s):
    // Horizontal bias calculation results in 36420, exceeding signed 16-bit max (32767).
    let ok4 = guard.update(36420);
    // Overflow caught before conversion, hardware exception prevented:
    assert!(!ok4);
    assert!(guard.is_overflow_detected());
    assert!(guard.is_backup_engaged());
    // Previous valid bias preserved:
    assert!(guard.current_bias_scaled() == 31500);
    assert!(guard.valid_sample_count() == 3);

    // 4. Negative overflow boundary check:
    let mut neg_guard = BiasOverflowGuard::new(32767);
    let neg_ok = neg_guard.update(-40000);
    assert!(!neg_ok);
    assert!(neg_guard.is_overflow_detected());
    assert!(neg_guard.is_backup_engaged());

    // 5. Recovery / Reset check:
    let reset_ok = guard.reset();
    assert!(reset_ok);
    assert!(!guard.is_overflow_detected());
    assert!(!guard.is_backup_engaged());
    assert!(guard.current_bias_scaled() == 0);
    assert!(guard.valid_sample_count() == 0);

    println!("Ariane 501 bias conversion overflow guard verified successfully.");
    42
}
