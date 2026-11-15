// Example 53: Rosetta / Philae Comet Landing Dual-Corroboration Cold-Gas Thruster and Anchor Harpoon Interlock
// Prevents microgravity bounce across comet surface by requiring dual corroborated gear strain and anchor penetration telemetry.
// Inspired by spacecraft touchdown and anchoring safety systems in RT-Thread (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize ColdGasAnchoringInterlock:
    // Min gear damping strain = 50 N, Min anchor penetration depth = 100 mm
    let mut interlock = ColdGasAnchoringInterlock::new(50, 100);
    assert!(!interlock.is_corroborated());
    assert!(!interlock.is_thruster_armed());
    assert!(!interlock.is_harpoon_fired());
    assert!(!interlock.is_bounce_mitigation_active());
    assert!(interlock.interlock_trip_count() == 0);

    // 2. Free-fall descent in microgravity:
    // No gear contact (0 N), no surface penetration (0 mm), no firing requested
    let ok1 = interlock.evaluate_touchdown(0, 0, false, false);
    assert!(!ok1);
    assert!(!interlock.is_corroborated());
    assert!(interlock.interlock_trip_count() == 0);

    // 3. Premature in-flight firing attempt rejection:
    // False sensor noise (10 N strain, 0 mm penetration) with premature firing commands
    let ok2 = interlock.evaluate_touchdown(10, 0, true, true);
    assert!(!ok2);
    assert!(!interlock.is_corroborated());
    assert!(!interlock.is_thruster_armed());
    assert!(!interlock.is_harpoon_fired());
    assert!(!interlock.is_bounce_mitigation_active());
    assert!(interlock.interlock_trip_count() == 1);

    // 4. Philae actual touchdown failure scenario:
    // Landing gear touches down with 65 N strain (>= 50 N),
    // but ice/dust crust yields insufficient anchor penetration (20 mm < 100 mm).
    // Firing commands commanded without dual corroboration:
    let ok3 = interlock.evaluate_touchdown(65, 20, true, true);
    assert!(!ok3);
    assert!(!interlock.is_corroborated());
    assert!(!interlock.is_bounce_mitigation_active());
    assert!(interlock.interlock_trip_count() == 2);

    // 5. Nominal dual-corroborated anchor lock:
    // Damping strain reaches 85 N (>= 50 N) AND surface penetrators anchor at 140 mm (>= 100 mm):
    let ok4 = interlock.evaluate_touchdown(85, 140, true, true);
    assert!(ok4);
    assert!(interlock.is_corroborated());
    assert!(interlock.is_thruster_armed());
    assert!(interlock.is_harpoon_fired());
    assert!(interlock.is_bounce_mitigation_active());
    // Trip count does not increment on valid corroborated actuation:
    assert!(interlock.interlock_trip_count() == 2);
    assert!(interlock.current_gear_strain_n() == 85);
    assert!(interlock.current_penetration_mm() == 140);

    // 6. Reset verification:
    let reset_ok = interlock.reset();
    assert!(reset_ok);
    assert!(!interlock.is_corroborated());
    assert!(!interlock.is_thruster_armed());
    assert!(!interlock.is_harpoon_fired());
    assert!(!interlock.is_bounce_mitigation_active());
    assert!(interlock.interlock_trip_count() == 0);

    42
}
