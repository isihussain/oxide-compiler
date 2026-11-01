// Error: calling mutating update on immutable HoldDownInterlock
fn main() -> i32 {
    let interlock = HoldDownInterlock::new(0, 5000);
    interlock.update(4000, 0, true);
    0
}
