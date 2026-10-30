// Static corridor breach on trajectory position x exceeding corridor limits
fn main() -> i32 {
    let corridor = SafetyCorridor::new(-2000, 2000, 0, 50000, 0, 80000, 2500);
    corridor.verify_or_terminate(3500, 1000, 2000, 0, 10, 20);
    0
}
