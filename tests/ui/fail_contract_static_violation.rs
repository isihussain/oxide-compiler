#[requires(thrust_kn >= 100 && thrust_kn <= 1200)]
fn fire_thruster(thrust_kn: i32) -> i32 {
    thrust_kn
}

fn main() {
    fire_thruster(50);
}
