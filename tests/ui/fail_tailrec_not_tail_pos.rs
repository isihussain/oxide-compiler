// Test failure: #[tailrec] function contains recursive call that is not in tail position

#[tailrec]
fn bad_factorial(n: i32) -> i32 {
    if n <= 1 {
        1
    } else {
        n * bad_factorial(n - 1)
    }
}

fn main() -> i32 {
    bad_factorial(5)
}
