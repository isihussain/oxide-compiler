// Constant-Bounded Loop Unrolling & Pure Leaf Function Inline Expansion
// Verifies compile-time expansion of pure functions and static unrolling of loops
// with constant RangeExpr bounds.

#[inline]
fn square(x: i32) -> i32 {
    x * x
}

fn min_i32(a: i32, b: i32) -> i32 {
    if a < b { a } else { b }
}

fn scale_offset(val: i32, factor: i32, offset: i32) -> i32 {
    return val * factor + offset;
}

fn main() -> i32 {
    // 1. Pure function inline expansion and compile-time evaluation
    let sq: i32 = square(5);                  // Inlines 5 * 5 -> folds to 25
    let smallest: i32 = min_i32(10, 80);      // Inlines if 10 < 80 { 10 } else { 80 } -> folds to 10
    let scaled: i32 = scale_offset(4, 3, 5);  // Inlines 4 * 3 + 5 -> folds to 17

    // 2. Constant-bounded loop unrolling (trip count 4 <= 8)
    let mut kernel: [i32; 4] = [0, 0, 0, 0];
    for i in 0..4 {
        kernel[i] = i * 10;
    }

    // 3. Dead loop elimination (start >= end: 0 iterations executed)
    let mut dead_guard: i32 = 0;
    for k in 8..3 {
        dead_guard = dead_guard + 999;
    }

    let loop_sum: i32 = kernel[0] + kernel[1] + kernel[2] + kernel[3]; // 0 + 10 + 20 + 30 = 60
    let base: i32 = loop_sum - sq; // 60 - 25 = 35
    let adjusted: i32 = base + scaled; // 35 + 17 = 52
    let result: i32 = adjusted - smallest + dead_guard; // 52 - 10 + 0 = 42

    return result;
}
