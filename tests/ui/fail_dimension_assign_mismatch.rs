// Negative test: Incompatible physical dimension binding.
// Assigning a length (Meter) variable to a time (Second) variable must be rejected statically.

fn main() -> i32 {
    let distance: Meter = 1000;
    let time: Second = distance; // Error: mismatched types in 'let': expected 'Second', found 'Meter'
    0
}
