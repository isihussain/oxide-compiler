// Constant Folding and Dead Code Elimination
// Evaluates constant expressions at compile-time and eliminates dead code following return.

fn compute_answer() -> i32 {
    let folded_arithmetic: i32 = 10 + 20 * 2 - 8 / 2; // 46
    let offset: i32 = -4;
    let result: i32 = folded_arithmetic + offset; // 42
    return result;

    // Dead code: eliminated by AST optimizer
    let unreachable_var: i32 = 100;
    return unreachable_var;
}

fn main() -> i32 {
    return compute_answer();
}
