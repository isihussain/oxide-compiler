// Violation: Mismatched payload type when constructing enum variant
enum Result {
    Ok(i32),
    Err,
}

fn main() -> i32 {
    let r = Result::Ok(true);
    0
}
