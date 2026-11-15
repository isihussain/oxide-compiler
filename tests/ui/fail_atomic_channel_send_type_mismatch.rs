fn main() -> i32 {
    let mut ch = AtomicChannel::new();
    ch.send(true);
    0
}
