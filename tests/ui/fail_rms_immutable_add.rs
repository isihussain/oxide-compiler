fn main() -> i32 {
    let sched = RmsScheduler::new(8);
    sched.add_task(1, 10000, 2000);
    0
}
