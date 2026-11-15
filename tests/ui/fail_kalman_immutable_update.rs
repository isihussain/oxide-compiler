// Error: calling mutating update on immutable DescentKalmanGate
fn main() -> i32 {
    let gate = DescentKalmanGate::new(250, 100);
    gate.update(20, 1000, 1000);
    0
}
