// Test failure: #[tailrec] function contains no recursive calls

#[tailrec]
fn non_recursive_fn(x: i32) -> i32 {
    x + 1
}

fn main() -> i32 {
    non_recursive_fn(41)
}
