#[max_stack_bytes(0)]
fn invalid_stack_task() -> i32 {
    42
}

fn main() {
    invalid_stack_task();
}
