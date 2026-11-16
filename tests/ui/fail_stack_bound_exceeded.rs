#[max_stack_bytes(64)]
fn large_frame_task() -> i32 {
    let large_buffer: [i32; 32] = [0; 32];
    large_buffer[0]
}

fn main() {
    large_frame_task();
}
