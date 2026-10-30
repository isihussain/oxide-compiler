// Negative UI Test: Mismatched types in fixed-point arithmetic
// Q16_16 cannot be directly added to i32 without conversion

fn main() {
    let a = Q16_16::from_int(10);
    let b = a + 5;
}
