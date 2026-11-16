#[ensures(bad_output > 0)]
fn compute_trajectory(vel_mps: i32) -> i32 {
    vel_mps
}

fn main() {
    compute_trajectory(100);
}
