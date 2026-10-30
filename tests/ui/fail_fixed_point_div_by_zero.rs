// Negative UI Test: Fixed-point division by zero compile-time error
// Divisor is static zero Q16_16::from_int(0)

fn main() {
    let a = Q16_16::from_int(10);
    let b = a / Q16_16::from_int(0);
}
