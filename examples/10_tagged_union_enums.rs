// Test user-defined tagged union enums and match pattern matching
enum Status {
    Pending,
    Active,
    Completed,
}

enum Option {
    Some(i32),
    None,
}

fn status_code(s: Status) -> i32 {
    match s {
        Status::Pending => 1,
        Status::Active => 2,
        Status::Completed => 3,
    }
}

fn unwrap_or(opt: Option, default_val: i32) -> i32 {
    match opt {
        Option::Some(x) => x,
        Option::None => default_val,
    }
}

fn main() -> i32 {
    let s = Status::Active;
    let code = status_code(s);

    let opt = Option::Some(40);
    let val = unwrap_or(opt, 0);

    code + val
}
