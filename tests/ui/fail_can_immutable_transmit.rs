fn main() -> i32 {
    let bus = CanAerospaceBus::new(1, 500, 50);
    bus.transmit_frame(100, 1, 8, 0);
    0
}
