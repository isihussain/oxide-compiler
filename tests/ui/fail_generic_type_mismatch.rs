// Violation: Generic function type argument mismatch
fn pick<T>(a: T, b: T) -> T {
    a
}

fn main() -> i32 {
    pick(42, true)
}
