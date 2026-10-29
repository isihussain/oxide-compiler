// Negative test: Dead code following a return statement must still pass type checking.

fn unreachable_type_error() -> i32 {
    return 42;
    let bad: i32 = true; // Error: mismatched types in 'let': expected 'i32', found 'bool'
}

fn main() -> i32 {
    unreachable_type_error()
}
