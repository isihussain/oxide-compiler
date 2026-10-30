// Negative UI Test: Worst-Case Execution Time (WCET) budget exceeded under --profile=aerospace
// The function contains a 100-iteration loop whose execution exceeds the 1 us budget.

#[realtime(budget_us = 1)]
fn guidance_deadline_exceeded() -> i32 {
    let mut sum = 0;
    for i in 0..100 {
        sum = sum + i * 2;
    }
    sum
}

fn main() {
    guidance_deadline_exceeded();
}
