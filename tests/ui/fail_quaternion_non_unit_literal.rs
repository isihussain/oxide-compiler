// Error: non-unit quaternion literal deviates from unit scale
fn main() -> i32 {
    let q = Quaternion::new(1000, 500, 0, 0);
    0
}
