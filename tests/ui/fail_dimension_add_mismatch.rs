// Negative test: Dimensionally inconsistent addition (NASA Mars Climate Orbiter failure pattern).
// Adding length (Meter) and time (Second) must be rejected statically at compile time.

fn main() -> i32 {
    let altitude: Meter = 1000;
    let duration: Second = 60;
    let invalid = altitude + duration; // Error: cannot add differing physical dimensions 'Meter' and 'Second'
    0
}
