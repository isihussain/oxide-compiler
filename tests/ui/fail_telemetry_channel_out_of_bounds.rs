// TelemetryPacket channel index exceeding 0..3 bounds
fn main() -> i32 {
    let packet = TelemetryPacket::new(1, 100, 10, 20, 30, 40);
    let val = packet.channel(4);
    0
}
