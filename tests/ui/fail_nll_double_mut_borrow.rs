fn main() -> i32 {
    let mut x = 10;
    let r1 = &mut x;
    let r2 = &mut x;
    *r1 = 20;
    *r2 = 30;
    x
}
