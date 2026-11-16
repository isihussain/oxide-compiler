fn child_worker() -> i32 {
    let buf: [i32; 64] = [0; 64];
    buf[0]
}

#[max_stack_bytes(128)]
fn parent_task() -> i32 {
    let x: i32 = 1;
    x + child_worker()
}

fn main() {
    parent_task();
}
