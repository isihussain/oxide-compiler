// Example 38: Liquid Rocket Engine Turbopump Cavitation and Shaft Speed Surveillance
// Mitigates hydraulic cavitation and blade resonance disintegration (Long March 5 / YF-77 incident mitigation).
// Inspired by RT-Thread RTOS aerospace sensor monitoring architectures (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize TurbopumpMonitor:
    // min_rpm = 10000, max_rpm = 25000, min_delta_p = 1500 kPa, cavitation_limit = 3 consecutive frames
    let mut tp = TurbopumpMonitor::new(10000, 25000, 1500, 3);
    assert!(tp.is_nominal());
    assert!(tp.state() == 0);
    assert!(tp.cavitation_count() == 0);

    // 2. Nominal mainstage burn:
    // shaft_rpm = 18000, inlet_p = 300 kPa, discharge_p = 2500 kPa (delta_p = 2200 kPa > 1500)
    let s1 = tp.update(18000, 300, 2500);
    assert!(s1 == 0);
    assert!(tp.is_nominal());
    assert!(tp.delta_p() == 2200);
    assert!(tp.shaft_rpm() == 18000);
    assert!(tp.cavitation_count() == 0);

    // 3. Transient pressure perturbation (e.g. vapor bubble ingestion):
    // discharge drops to 1500 kPa -> delta_p = 1200 kPa (< 1500 kPa min_delta_p)
    // Frame 1 triggers warning state (1) for engine throttle adjustment
    let s2 = tp.update(18200, 300, 1500);
    assert!(s2 == 1);
    assert!(tp.is_warning());
    assert!(tp.cavitation_count() == 1);

    // 4. Hydraulic recovery: discharge pressure restores to 2500 kPa
    let s3 = tp.update(18100, 300, 2500);
    assert!(s3 == 0);
    assert!(tp.is_nominal());
    assert!(tp.cavitation_count() == 0);

    // 5. Severe persistent cavitation collapse:
    // Delta-P collapses below minimum threshold for 3 consecutive frames
    let c1 = tp.update(18500, 300, 1400); // Frame 1: warning
    assert!(c1 == 1);
    assert!(tp.is_warning());
    assert!(tp.cavitation_count() == 1);

    let c2 = tp.update(18600, 300, 1350); // Frame 2: warning
    assert!(c2 == 1);
    assert!(tp.is_warning());
    assert!(tp.cavitation_count() == 2);

    let c3 = tp.update(18800, 300, 1300); // Frame 3: exceeds limit -> SHUTDOWN commanded
    assert!(c3 == 2);
    assert!(tp.is_shutdown());
    assert!(tp.trip_code() == 3); // CAVITATION_COLLAPSE

    // 6. Lockout verification: subsequent frames remain latched in shutdown
    let c4 = tp.update(18000, 300, 2500);
    assert!(c4 == 2);
    assert!(tp.is_shutdown());

    // 7. Overspeed protection test on separate monitor instance:
    let mut tp_overspeed = TurbopumpMonitor::new(10000, 25000, 1500, 3);
    let os = tp_overspeed.update(26500, 300, 2500); // 26500 > max_rpm 25000
    assert!(os == 2);
    assert!(tp_overspeed.is_shutdown());
    assert!(tp_overspeed.trip_code() == 1); // OVERSPEED

    println!("YF-77 turbopump cavitation and shaft speed surveillance verified successfully.");
    42
}
