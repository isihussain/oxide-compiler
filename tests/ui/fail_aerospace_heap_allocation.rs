extern "C" {
    fn malloc(size: i32) -> i32;
}

fn main() -> i32 {
    let p = malloc(64);
    0
}
