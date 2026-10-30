// Error: calling mutating publish on immutable SoftwareBus
fn main() -> i32 {
    let bus = SoftwareBus::new();
    bus.publish(0, 10);
    0
}
