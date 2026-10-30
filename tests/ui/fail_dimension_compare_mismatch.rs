// Negative test: Dimensionally inconsistent relational comparison.
// Comparing length (Meter) with force (Newton) must be rejected statically.

fn main() -> i32 {
    let altitude: Meter = 5000;
    let thrust: Newton = 2000;
    if altitude > thrust { // Error: cannot compare differing physical dimensions 'Meter' and 'Newton'
        return 1;
    }
    0
}
