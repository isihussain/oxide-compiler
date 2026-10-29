// Violation: Mutual / indirect recursion violates NASA/JPL Flight Software Rule 3
fn ping(n: i32) -> i32 {
    pong(n - 1)
}

fn pong(n: i32) -> i32 {
    if n <= 0 {
        0
    } else {
        ping(n)
    }
}

fn main() -> i32 {
    ping(5)
}
