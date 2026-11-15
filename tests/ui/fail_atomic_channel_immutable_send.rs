fn main() -> i32 {
    let ch = AtomicChannel::new();
    ch.send(10);
    0
}
