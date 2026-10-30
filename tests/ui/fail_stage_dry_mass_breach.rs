// Burning propellant in excess of remaining propellant mass breaching structural dry mass
fn main() -> i32 {
    let mut stage = StageMass::new(25000, 50000);
    stage.burn(60000);
    0
}
