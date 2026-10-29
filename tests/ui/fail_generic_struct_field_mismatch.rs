// Violation: Generic struct field type mismatch
struct Pair<T> {
    first: T,
    second: T,
}

fn main() -> i32 {
    let p = Pair { first: 42, second: false };
    0
}
