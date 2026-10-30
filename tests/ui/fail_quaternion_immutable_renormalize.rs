// Error: calling mutating renormalize on immutable quaternion
fn main() -> i32 {
    let q = Quaternion::identity();
    q.renormalize();
    0
}
