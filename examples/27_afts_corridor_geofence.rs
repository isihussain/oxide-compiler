// Example 27: Autonomous Flight Termination System (AFTS) Safety Corridor Geofencing
//
// Modern orbital launch systems (SpaceX Falcon 9, NASA SLS) employ autonomous flight
// safety computers to continuously evaluate position and velocity state vectors against
// designated range safety corridor bounds without requiring human ground intervention.
// If the vehicle breaches corridor boundaries or velocity limits, flight termination is commanded.

fn main() -> i32 {
    // Cape Canaveral / Starbase safety corridor:
    // Crossrange x: [-2000, 2000] meters (+/- 2 km)
    // Downrange y:  [0, 50000] meters (0 to 50 km)
    // Altitude z:   [0, 80000] meters (0 to 80 km)
    // Max velocity: 2500 m/s (Mach 7.5 boundary)
    let corridor = SafetyCorridor::new(-2000, 2000, 0, 50000, 0, 80000, 2500);

    // 1. Initial ascent check:
    let r1 = corridor.is_inside(0, 100, 500);
    if !r1 {
        return 1;
    }
    let c1 = corridor.check(0, 100, 500, 0, 20, 150);
    if !c1 {
        return 2;
    }
    let t1 = corridor.verify_or_terminate(0, 100, 500, 0, 20, 150);
    if t1 != 0 {
        return 3;
    }
    println!("AFTS liftoff gate: NOMINAL");

    // 2. High dynamic pressure (Max-Q) downrange position:
    let c2 = corridor.check(200, 15000, 12000, 10, 800, 600);
    if !c2 {
        return 4;
    }
    let t2 = corridor.verify_or_terminate(200, 15000, 12000, 10, 800, 600);
    if t2 != 0 {
        return 5;
    }
    println!("AFTS Max-Q gate: NOMINAL");

    // 3. Statically-checked non-fatal query on out-of-corridor crossrange breach:
    let r3 = corridor.is_inside(-3500, 10000, 15000);
    if r3 {
        return 6;
    }
    println!("AFTS crossrange geofence detection: CONFIRMED");

    // 4. Statically-checked non-fatal query on excessive velocity breach:
    // v^2 = 2000^2 + 2000^2 = 8,000,000 > 2500^2 = 6,250,000
    let c4 = corridor.check(0, 20000, 25000, 2000, 2000, 0);
    if c4 {
        return 7;
    }
    println!("AFTS velocity envelope detection: CONFIRMED");

    println!("All Autonomous Flight Termination System corridor checks verified.");
    42
}
