// Example 41: Aerodynamic Control Surface Encoder Stall Detection and Authority Re-Allocation
// Mitigates mechanical grid fin freeze and aerodynamic control loss (i-Space Hyperbola-1 incident mitigation).
// Inspired by RT-Thread RTOS actuator monitoring and authority re-allocation (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize ControlSurfaceGuard:
    // 4 grid fins, max tracking error = 150 cdeg (1.5 deg), stall limit = 3 frames
    let mut guard = ControlSurfaceGuard::new(4, 150, 3);
    assert!(!guard.is_surface_isolated());
    assert!(guard.jammed_surface_id() == -1);
    assert!(!guard.rcs_assist_engaged());
    assert!(guard.active_fin_gain_percent() == 100);
    assert!(guard.stall_count() == 0);

    // 2. Nominal tracking on Fin 0: commanded 500 cdeg, measured 495 cdeg (error = 5 <= 150)
    let ok1 = guard.update(0, 500, 495);
    assert!(ok1);
    assert!(!guard.is_surface_isolated());
    assert!(guard.stall_count() == 0);

    // 3. Transient aerodynamic buffeting on Fin 1: commanded 400 cdeg, measured 200 cdeg (error = 200 > 150)
    let ok2 = guard.update(1, 400, 200);
    assert!(ok2); // Glitch tolerated on frame 1
    assert!(guard.stall_count() == 1);
    assert!(!guard.is_surface_isolated());

    // 4. Aerodynamic recovery on Fin 1: measured returns to 395 cdeg (error = 5 <= 150)
    let ok3 = guard.update(1, 400, 395);
    assert!(ok3);
    assert!(guard.stall_count() == 0);

    // 5. Mechanical actuator freeze on Fin 2 under max dynamic pressure (Hyperbola-1 incident scenario):
    // Fin 2 jams at 0 cdeg while guidance commands 300 cdeg
    let f1 = guard.update(2, 300, 0); // Frame 1: error = 300 > 150
    assert!(f1);
    assert!(guard.stall_count() == 1);

    let f2 = guard.update(2, 300, 0); // Frame 2: error = 300 > 150
    assert!(f2);
    assert!(guard.stall_count() == 2);

    let f3 = guard.update(2, 300, 0); // Frame 3: reaches limit -> Fin 2 declared jammed!
    assert!(!f3);
    assert!(guard.is_surface_isolated());
    assert!(guard.jammed_surface_id() == 2);

    // RCS reaction control thrusters engaged to supply counter-torque:
    assert!(guard.rcs_assist_engaged());

    // Control authority reallocated across 3 remaining active fins:
    // Scale factor = (100 * 4) / 3 = 133%
    assert!(guard.active_fin_gain_percent() == 133);

    // 6. Jammed fin lockout: subsequent commands to Fin 2 are rejected
    let f4 = guard.update(2, 300, 0);
    assert!(!f4);
    assert!(guard.is_surface_isolated());

    println!("Hyperbola-1 grid fin stall detection and authority re-allocation verified successfully.");
    42
}
