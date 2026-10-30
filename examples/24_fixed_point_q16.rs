// Example 24: Deterministic Fixed-Point Q16.16 Arithmetic for Backup Flight Controllers
//
// Backup flight control systems (e.g. secondary grid fin ESCs, abort controllers)
// avoid floating-point unit (FPU) dependencies to guarantee bit-exact deterministic
// execution across diverse backup microcontrollers.
//
// Q16.16 representation: 16-bit signed integer, 16-bit fractional precision (1 LSB = 1/65536).
// Features verified:
//  - Automatic 64-bit intermediate scaling for multiplication and division.
//  - Saturation arithmetic on overflow under --profile=aerospace.
//  - Bit-exact fractional representation and ordering comparisons.

fn main() -> i32 {
    let angle_cmd = Q16_16::from_int(15);
    let angle_meas = Q16_16::from_int(12);

    // Order comparison
    assert!(angle_meas < angle_cmd);
    assert!(angle_cmd > angle_meas);

    // Proportional control error calculation
    let error = angle_cmd - angle_meas;
    assert!(error.to_int() == 3);

    // Multiplicative gain scaling: 3 * 4 = 12
    let gain = Q16_16::from_int(4);
    let correction = error * gain;
    assert!(correction.to_int() == 12);

    // Division damping: 12 / 2 = 6
    let half_factor = Q16_16::from_int(2);
    let damped = correction / half_factor;
    assert!(damped.to_int() == 6);
    assert!(damped == Q16_16::from_int(6));

    // Bit-exact fractional division: 1 / 2 = 0.5 (raw: 32768)
    let one = Q16_16::from_int(1);
    let two = Q16_16::from_int(2);
    let half = one / two;
    assert!(half.raw() == 32768);

    // Fractional multiplication: 0.5 * 2 = 1.0
    let reconstructed_one = half * two;
    assert!(reconstructed_one.to_int() == 1);

    // Unary negation
    let neg_cmd = -angle_cmd;
    assert!(neg_cmd.to_int() == -15);

    // Explicit saturation methods
    let max_q = Q16_16::from_raw(2147483647);
    let sat_add = max_q.saturating_add(Q16_16::from_int(10));
    assert!(sat_add.raw() == 2147483647);

    let min_q = Q16_16::from_raw(-2147483648);
    let sat_sub = min_q.saturating_sub(Q16_16::from_int(10));
    assert!(sat_sub.raw() == -2147483648);

    // Saturation under --profile=aerospace
    let aero_sat = max_q + Q16_16::from_int(100);
    assert!(aero_sat.raw() == 2147483647);

    println!("Deterministic Q16.16 fixed-point ESC control verified.");

    // Exit code: 6 + 36 = 42
    damped.to_int() + 36
}
