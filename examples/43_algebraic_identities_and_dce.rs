// Algebraic Identity Simplification & Dead Code Elimination
// Verifies compile-time identity folding (x+0, x-0, x*1, x*0, x/1), double negation (-(-x), !(!x)),
// and static dead-code elimination on constant branches.

fn verify_identities(base: i32) -> i32 {
    let step1: i32 = base + 0;      // x + 0 -> x
    let step2: i32 = 0 + step1;     // 0 + x -> x
    let step3: i32 = step2 - 0;     // x - 0 -> x
    let step4: i32 = step3 * 1;     // x * 1 -> x
    let step5: i32 = 1 * step4;     // 1 * x -> x
    let step6: i32 = step5 / 1;     // x / 1 -> x
    let zero_val: i32 = base * 0;   // x * 0 -> 0

    let un_neg: i32 = -(-step6);    // -(-x) -> x
    let flag: bool = true;
    let flag_clean: bool = !(!flag); // !(!x) -> x

    if flag_clean {
        // True branch taken
    } else {
        // Dead branch pruned by optimizer
        return 999;
    }

    if false {
        // Dead branch pruned by optimizer
        return 888;
    }

    while false {
        // Dead loop body pruned by optimizer
        return 777;
    }

    let final_res: i32 = un_neg + zero_val;
    return final_res;

    // Dead code following return pruned by optimizer
    let dead_tail: i32 = 12345;
    return dead_tail;
}

fn main() -> i32 {
    let result: i32 = verify_identities(42);
    return result;
}
