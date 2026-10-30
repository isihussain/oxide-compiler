// Error: software bus channel index out of static bounds [0..7]
fn main() -> i32 {
    let mut bus = SoftwareBus::new();
    bus.subscribe(9);
    0
}
