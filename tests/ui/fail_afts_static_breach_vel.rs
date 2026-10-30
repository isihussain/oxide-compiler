// Static corridor breach on velocity magnitude exceeding corridor limit
fn main() -> i32 {
    let corridor = SafetyCorridor::new(-2000, 2000, 0, 50000, 0, 80000, 100);
    corridor.verify_or_terminate(0, 1000, 2000, 80, 80, 0);
    0
}
