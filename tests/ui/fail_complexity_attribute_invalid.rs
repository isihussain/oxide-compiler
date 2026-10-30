// Function with invalid non-positive max_complexity attribute
#[max_complexity = 0]
fn zero_complexity_task() -> i32 {
    0
}

fn main() -> i32 {
    zero_complexity_task();
    0
}
