extern "C" {
    fn abs(x: i32) -> i32;
}

fn compute_diff(a: i32, b: i32) -> i32 {
    abs(a - b)
}

fn main() -> i32 {
    let diff: i32 = compute_diff(10, 52);
    diff
}
