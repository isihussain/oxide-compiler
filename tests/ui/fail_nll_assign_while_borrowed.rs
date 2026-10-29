fn main() -> i32 {
    let mut x = 10;
    let r = &x;
    x = 20;
    let v = *r;
    v
}
