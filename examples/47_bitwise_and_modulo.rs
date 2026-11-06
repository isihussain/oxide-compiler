// Milestone 13 Conformance Test: Bitwise and Modulo Operations
// Verifies modulo remainder (%), bitwise AND (&), OR (|), XOR (^), shifts (<<, >>),
// logical short-circuit conjunctions (&&, ||), and constant folding algebraic reductions.

fn test_modulo() -> i32 {
    let a: i32 = 142;
    let b: i32 = 100;
    let remainder: i32 = a % b; // 42
    remainder
}

fn test_bitwise_logic() -> i32 {
    // 0xAA = 170, 0x55 = 85
    let mask: i32 = 170;
    let val: i32 = 255;
    let bit_and: i32 = val & mask; // 170
    let bit_xor: i32 = bit_and ^ 128; // 170 ^ 128 = 42
    bit_xor
}

fn test_bitwise_shifts() -> i32 {
    let base: i32 = 21;
    let shifted_left: i32 = base << 1; // 42
    let shifted_right: i32 = (shifted_left << 2) >> 2; // 42
    shifted_right
}

fn test_boolean_logic() -> bool {
    let t1: bool = (10 < 20) && (30 < 40); // true
    let t2: bool = (10 > 20) || (50 == 50); // true
    let f1: bool = (10 > 20) && (30 < 40); // false
    let f2: bool = (10 > 20) || (30 > 40); // false

    t1 && t2 && !f1 && !f2
}

fn main() -> i32 {
    let m: i32 = test_modulo();
    let b: i32 = test_bitwise_logic();
    let s: i32 = test_bitwise_shifts();
    let ok: bool = test_boolean_logic();

    if ok && (m == 42) && (b == 42) && (s == 42) {
        42
    } else {
        1
    }
}
