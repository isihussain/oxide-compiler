// Violation: Incompatible types in match arms
enum Status {
    Active,
    Inactive,
}

fn test(s: Status) -> i32 {
    match s {
        Status::Active => 42,
        Status::Inactive => true,
    }
}

fn main() -> i32 {
    test(Status::Active)
}
