// Burning negative propellant amount
fn main() -> i32 {
    let mut stage = StageMass::new(25000, 50000);
    stage.burn(-1000);
    0
}
