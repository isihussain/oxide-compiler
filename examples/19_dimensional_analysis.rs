// Dimensional Analysis & Physical SI Units (Mission-Critical Aerospace Flight Safety)
// Prevents unit mismatch errors (e.g. Mars Climate Orbiter failure) with zero runtime overhead.

fn compute_altitude(start_alt: Meter, delta_alt: Meter) -> Meter {
    start_alt + delta_alt
}

fn compute_velocity(dist: Meter, duration: Second) -> Mps {
    dist / duration
}

fn compute_thrust(mass: Kilogram, accel: Mps2) -> Newton {
    mass * accel
}

fn main() -> i32 {
    let initial_altitude: Meter = 1000;
    let stage1_climb: Meter = 4000;
    let current_alt: Meter = compute_altitude(initial_altitude, stage1_climb); // 5000 Meter

    let t: Second = 100;
    let avg_vel: Mps = compute_velocity(current_alt, t); // 50 Mps

    let dry_mass: Kilogram = 200;
    let burn_accel: Mps2 = 10;
    let engine_thrust: Newton = compute_thrust(dry_mass, burn_accel); // 2000 Newton

    // Dimensional scaling with scalar integer
    let scaled_thrust: Newton = engine_thrust * 2; // 4000 Newton
    let half_thrust: Newton = scaled_thrust / 2;   // 2000 Newton

    // Comparison within identical physical dimensions
    let mut status: i32 = 0;
    if current_alt > initial_altitude {
        status = status + 10;
    }

    if engine_thrust == half_thrust {
        status = status + 32;
    }

    // Status: 10 + 32 = 42
    status
}
