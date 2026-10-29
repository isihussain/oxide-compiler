extern "C" {
    fn abs(x: i32) -> i32;
}

fn main() -> i32 {
    let flag: bool = true;
    abs(flag)
}
