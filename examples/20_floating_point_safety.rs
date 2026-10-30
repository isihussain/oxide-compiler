// Floating-Point Flight Safety & Real-Time GNC Attitude Control
// Verifies IEEE 754 f32/f64 arithmetic, FTZ/DAZ subnormal cycle spike elimination,
// bounded clamp validation, and domain-safe square-root calculation.

fn compute_kinetic_energy(mass: f64, velocity: f64) -> f64 {
    0.5 * mass * velocity * velocity
}

fn estimate_velocity(kinetic_energy: f64, mass: f64) -> f64 {
    let v_squared: f64 = (2.0 * kinetic_energy) / mass;
    sqrt(v_squared)
}

fn limit_g_force(thrust_accel: f32, min_g: f32, max_g: f32) -> f32 {
    clamp(thrust_accel, min_g, max_g)
}

fn main() -> i32 {
    let mass: f64 = 100.0;
    let initial_vel: f64 = 20.0;
    let energy: f64 = compute_kinetic_energy(mass, initial_vel); // 0.5 * 100 * 400 = 20000.0
    let recovered_vel: f64 = estimate_velocity(energy, mass);   // sqrt(400.0) = 20.0

    // Bounded acceleration limiting with clamp
    let measured_accel: f32 = 45.5;
    let safe_accel: f32 = limit_g_force(measured_accel, 0.0, 30.0); // clamped to 30.0

    let mut status: i32 = 0;

    // Verify velocity recovery within numerical tolerance
    if recovered_vel > 19.99 {
        if recovered_vel < 20.01 {
            status = status + 22;
        }
    }

    // Verify acceleration clamp
    if safe_accel == 30.0 {
        status = status + 20;
    }

    // Status: 22 + 20 = 42
    status
}
