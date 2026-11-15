fn main() -> i32 {
    let mut bus = MilStd1553Bus::new(0, 3);
    bus.send_command(1, 1, 0);
    0
}
