// Violation: Direct recursion violates NASA/JPL Flight Software Rule 3
fn countdown(n: i32) -> i32 {
    if n <= 0 {
        0
    } else {
        countdown(n - 1)
    }
}

fn main() -> i32 {
    countdown(10)
}
