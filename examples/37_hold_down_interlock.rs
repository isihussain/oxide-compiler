// Example 37: Ground Test Stand Hold-Down Interlock and Inadvertent Liftoff Abort Guard
// Mitigates static-fire release failures (Space Pioneer Tianlong-3 incident mitigation).
// Inspired by RT-Thread RTOS aerospace interlock mechanisms (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize HoldDownInterlock in StaticFire mode (0) with 5000 kN rated clamp strain
    let mut ground_interlock = HoldDownInterlock::new(0, 5000);
    assert!(ground_interlock.is_nominal());
    assert!(!ground_interlock.is_thrust_terminated());
    assert!(ground_interlock.abort_reason() == 0);

    // 2. Nominal engine ramp-up during static firing:
    // Strain rises to 4500 kN, acceleration is 0 cm/s^2, umbilical connected
    let ok1 = ground_interlock.update(4500, 0, true);
    assert!(ok1);
    assert!(ground_interlock.is_nominal());
    assert!(ground_interlock.clamp_strain() == 4500);
    assert!(ground_interlock.accel_z() == 0);

    // 3. Structural detachment / premature release scenario (clamp strain drops to 0):
    // Static fire mode immediately latches emergency thrust cutoff
    let ok2 = ground_interlock.update(0, 0, true);
    assert!(!ok2);
    assert!(ground_interlock.is_thrust_terminated());
    assert!(ground_interlock.abort_reason() == 1); // CLAMP_RELEASE

    // 4. Reset test stand interlock after clearing ground anomaly
    ground_interlock.update(4500, 0, true); // Still blocked while terminated
    assert!(ground_interlock.is_thrust_terminated());
    let reset_success = ground_interlock.reset_abort();
    assert!(reset_success);
    assert!(ground_interlock.is_nominal());
    assert!(ground_interlock.abort_reason() == 0);

    // 5. Inadvertent liftoff detection: vertical acceleration spikes (120 cm/s^2)
    // Emergency thrust termination commands immediate engine shutdown
    let ok3 = ground_interlock.update(4200, 120, true);
    assert!(!ok3);
    assert!(ground_interlock.is_thrust_terminated());
    assert!(ground_interlock.abort_reason() == 2); // INADVERTENT_LIFTOFF

    // 6. Flight Launch mode (mode = 1):
    // Clamp release and vertical liftoff are nominal at T-0
    let mut flight_interlock = HoldDownInterlock::new(1, 5000);
    assert!(flight_interlock.is_nominal());

    // Hold-down clamp releases at liftoff: clamp_strain = 0, accel_z = 250 cm/s^2, umbilical = false
    let flight_ok = flight_interlock.update(0, 250, false);
    assert!(flight_ok);
    assert!(flight_interlock.is_nominal());
    assert!(!flight_interlock.is_thrust_terminated());

    println!("Hold-down clamp interlock and liftoff abort guard verified successfully.");
    42
}
