fn main() -> i32 {
    let mut bus = CanAerospaceBus::new(1, 500, 50);
    bus.transmit_frame(400, 0, 8, 100);
    0
}
