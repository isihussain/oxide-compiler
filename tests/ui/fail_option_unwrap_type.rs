enum Option {
    Some(i32),
    None,
}

fn main() -> i32 {
    let opt = Option::Some(10);
    let s: &str = opt.unwrap();
    0
}
