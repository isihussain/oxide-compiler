fn main() -> i32 {
    let mut x = 10;
    let r = &x;
    let mut i = 0;
    while i < 5 {
        x = x + 1;
        i = i + 1;
    }
    let v = *r;
    v
}
