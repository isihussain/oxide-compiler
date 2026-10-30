// Function exceeding declared cyclomatic complexity budget (M > threshold)
#[max_complexity = 3]
fn complex_flight_evaluator(a: i32, b: i32, c: i32, d: i32) -> i32 {
    let mut res = 0;
    if a > 0 {
        res = res + 1;
    }
    if b > 0 {
        res = res + 2;
    }
    if c > 0 {
        res = res + 4;
    }
    if d > 0 {
        res = res + 8;
    }
    res
}

fn main() -> i32 {
    complex_flight_evaluator(1, 2, 3, 4);
    0
}
