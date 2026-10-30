// Negative UI Test: Invalid non-positive real-time budget under --profile=aerospace

#[realtime(budget_us = 0)]
fn zero_budget_task() -> i32 {
    42
}

fn main() {
    zero_budget_task();
}
