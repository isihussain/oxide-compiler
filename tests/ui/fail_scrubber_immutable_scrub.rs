fn main() -> i32 {
    let scrubber = MemoryScrubber::new(256, 1000);
    scrubber.scrub_word(100);
    0
}
