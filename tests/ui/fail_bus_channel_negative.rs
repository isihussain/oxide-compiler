// Error: negative software bus message ID
fn main() -> i32 {
    let mut bus = SoftwareBus::new();
    bus.publish(-1, 42);
    0
}
