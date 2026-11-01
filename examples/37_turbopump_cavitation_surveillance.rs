// Example 37: Turbopump Shaft Speed and Cavitation Differential Pressure Surveillance
// Mitigates hydraulic cavitation thrust collapse and rotor disintegration (Long March 5 / YF-77 failure mitigation).

fn main() -> i32 {
    // 1. Initialize TurbopumpMonitor:
    // max_rpm: 35,000 RPM (critical structural vibration threshold)
    // min_delta_p: 5,000 kPa (minimum acceptable discharge head rise)
    // max_cavitation_ticks: 3 (consecutive low-pressure samples before emergency cutoff)
    let mut tp = TurbopumpMonitor::new(35000, 5000, 3);
    assert!(tp.is_nominal());
    assert!(tp.state() == 0);
    assert!(tp.command_thrust() == 100);
    assert!(tp.cavitation_ticks() == 0);

    // 2. Nominal mainstage burn: 28,000 RPM and 6,200 kPa head rise
    let s1 = tp.update(28000, 6200);
    let s2 = tp.update(28100, 6150);
    assert!(s1 == 0);
    assert!(s2 == 0);
    assert!(tp.is_nominal());
    assert!(tp.last_rpm() == 28100);
    assert!(tp.last_delta_p() == 6150);
    assert!(tp.command_thrust() == 100);

    // 3. Transient inlet bubble / pressure dip: delta_p drops to 4,500 kPa (< 5,000)
    // Monitor throttles engine to 60% to recover Net Positive Suction Head (NPSH)
    let s3 = tp.update(28200, 4500);
    assert!(s3 == 1);
    assert!(tp.is_throttled());
    assert!(tp.state() == 1);
    assert!(tp.command_thrust() == 60);
    assert!(tp.cavitation_ticks() == 1);

    // 4. Recovery: suction pressure restored (6,000 kPa)
    // Monitor returns to nominal 100% thrust
    let s4 = tp.update(28000, 6000);
    assert!(s4 == 0);
    assert!(tp.is_nominal());
    assert!(tp.command_thrust() == 100);
    assert!(tp.cavitation_ticks() == 0);

    // 5. Persistent cavitation event: 3 consecutive frames with collapsed delta_p
    tp.update(29000, 4200);
    assert!(tp.is_throttled());
    assert!(tp.cavitation_ticks() == 1);

    tp.update(29500, 3800);
    assert!(tp.is_throttled());
    assert!(tp.cavitation_ticks() == 2);

    tp.update(30000, 3500);
    assert!(tp.is_cutoff());
    assert!(tp.state() == 2);
    assert!(tp.command_thrust() == 0);
    assert!(tp.trip_reason() == 1);

    // 6. Cutoff latch lockout: subsequent updates remain locked in emergency shutdown
    let s_locked = tp.update(20000, 6000);
    assert!(s_locked == 2);
    assert!(tp.is_cutoff());
    assert!(tp.command_thrust() == 0);

    // 7. Turbine overspeed structural guard test
    let mut tp_overspeed = TurbopumpMonitor::new(35000, 5000, 3);
    assert!(tp_overspeed.is_nominal());
    let s_trip = tp_overspeed.update(36500, 5800);
    assert!(s_trip == 2);
    assert!(tp_overspeed.is_cutoff());
    assert!(tp_overspeed.command_thrust() == 0);
    assert!(tp_overspeed.trip_reason() == 2);

    println!("Turbopump cavitation surveillance verification successful");
    42
}
