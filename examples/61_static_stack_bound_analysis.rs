// Static Stack Usage Bound Analysis for Avionics SRAM Stacks
// Verifies local variable and cumulative call tree stack bounds under #[max_stack_bytes(N)]
// Inspired by NASA/JPL Flight Software Rule 1/2 and RT-Thread stack sizing guidelines

#[max_stack_bytes(128)]
fn leaf_computation(input: i32) -> i32 {
    let bias: i32 = 10;
    input + bias
}

#[max_stack_bytes(256)]
fn telemetry_processor(raw_sensor: i32) -> i32 {
    let intermediate: i32 = leaf_computation(raw_sensor);
    let scale: i32 = 2;
    intermediate * scale
}

#[max_stack_bytes(512)]
fn navigation_filter() -> i32 {
    let state_vector: [i32; 16] = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16];
    let filtered: i32 = telemetry_processor(state_vector[0]);
    filtered + 20
}

fn main() -> i32 {
    let res = navigation_filter();
    assert!(res == 42);
    42
}
