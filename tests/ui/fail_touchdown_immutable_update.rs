// Error: calling mutating update on immutable TouchdownInterlock
fn main() -> i32 {
    let gate = TouchdownInterlock::new(50, 5000);
    gate.update(100, 0);
    0
}
