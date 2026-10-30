// Example 36: Hierarchical Sensor Fault Detection, Isolation, and Recovery (NASA FDIR)
// Demonstrates persistent anomaly filtering, transient glitch suppression,
// deterministic state transitions (Healthy -> Degraded -> Isolated), and channel lockout.

fn main() -> i32 {
    // 1. Initialize FDIR for pitch rate gyro: [-500, +500] cdeg/s,
    // degrade after 3 consecutive faults, isolate after 6, recover after 4 healthy samples
    let mut fdir = SensorFDIR::new(-500, 500, 3, 6, 4);
    assert!(fdir.is_healthy());
    assert!(fdir.state() == 0);
    assert!(fdir.anomaly_count() == 0);

    // 2. Nominal telemetry feed: healthy sensor samples
    let s1 = fdir.update(100);
    let s2 = fdir.update(120);
    let s3 = fdir.update(110);
    assert!(s1 == 100);
    assert!(s2 == 120);
    assert!(s3 == 110);
    assert!(fdir.last_valid() == 110);
    assert!(fdir.is_healthy());

    // 3. Transient single-frame noise glitch (e.g. cosmic ray SEU bit-flip: 9999)
    let s4 = fdir.update(9999);
    // Transient anomaly filtered: returns last valid sample without tripping alarm
    assert!(s4 == 110);
    assert!(fdir.anomaly_count() == 1);
    assert!(fdir.is_healthy());

    // 4. Recovery from transient noise upon next valid frame
    let s5 = fdir.update(115);
    assert!(s5 == 115);
    assert!(fdir.anomaly_count() == 0);
    assert!(fdir.is_healthy());

    // 5. Persistent noise bursts: 3 consecutive anomalies drive Healthy -> Degraded
    fdir.update(9999);
    fdir.update(9999);
    fdir.update(9999);
    assert!(fdir.anomaly_count() == 3);
    assert!(fdir.is_degraded());
    assert!(fdir.state() == 1);

    // 6. Sensor health recovery: 4 consecutive valid samples return to Healthy
    fdir.update(120);
    fdir.update(120);
    fdir.update(120);
    fdir.update(120);
    assert!(fdir.is_healthy());
    assert!(fdir.state() == 0);

    // 7. Critical hard sensor failure: 6 consecutive anomalies drive to Isolated
    fdir.update(8888);
    fdir.update(8888);
    fdir.update(8888);
    assert!(fdir.is_degraded());

    fdir.update(8888);
    fdir.update(8888);
    fdir.update(8888);
    assert!(fdir.is_isolated());
    assert!(fdir.state() == 2);

    // 8. Isolated channel lockout: subsequent frames remain isolated and substituted
    let s_locked = fdir.update(100);
    assert!(s_locked == 120); // Last valid sample preserved
    assert!(fdir.is_isolated());

    println!("NASA Class-A Mission FDIR verification successful: state = Isolated ({})", fdir.state());
    42
}
