// Calling mutable push on immutable SpscRingBuffer instance
fn main() -> i32 {
    let ring = SpscRingBuffer::new(4);
    ring.push(42);
    0
}
