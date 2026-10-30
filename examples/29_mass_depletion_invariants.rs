// Example 29: Physical Dry Mass Lower Bound Invariants & Propellant Depletion
//
// In rocket guidance and throttle control (e.g. SpaceX Falcon 9 boostback and landing burns),
// vehicle mass must strictly remain at or above physical structural dry mass (m_total >= m_dry).
// Depleting below zero propellant causes negative mass or acceleration singularities (a = F/m).
// This example verifies compile-time and runtime tracking of structural dry mass invariants.

fn main() -> i32 {
    // Falcon 9 class first stage:
    // Structural dry mass: 25,000 kg
    // Usable propellant mass: 400,000 kg
    let mut booster = StageMass::new(25000, 400000);

    if booster.dry_mass() != 25000 {
        return 1;
    }
    if booster.propellant_mass() != 400000 {
        return 2;
    }
    if booster.total_mass() != 425000 {
        return 3;
    }
    if booster.is_depleted() {
        return 4;
    }

    // 1. Primary ascent burn: consume 350,000 kg propellant
    let m1 = booster.burn(350000);
    println!("Total mass after MECO: {} kg", m1);
    if m1 != 75000 {
        return 5;
    }
    if booster.propellant_mass() != 50000 {
        return 6;
    }
    if booster.is_depleted() {
        return 7;
    }

    // 2. Landing burn: consume remaining 50,000 kg propellant
    let m2 = booster.burn(50000);
    println!("Total mass after touchdown: {} kg", m2);
    if m2 != 25000 {
        return 8;
    }
    if booster.propellant_mass() != 0 {
        return 9;
    }
    if !booster.is_depleted() {
        return 10;
    }
    if booster.total_mass() != booster.dry_mass() {
        return 11;
    }

    println!("Physical dry mass lower bound invariants verified successfully.");
    42
}
