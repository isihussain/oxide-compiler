fn main() -> i32 {
    let mutex = PriorityCeilingMutex::new(30);
    mutex.lock(1, 10);
    0
}
