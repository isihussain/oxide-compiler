fn max(a: i32, b: i32) -> i32 {
    if a > b {
        a
    } else {
        b
    }
}

fn main() -> i32 {
    let result: i32 = max(42, 10);
    result
}
