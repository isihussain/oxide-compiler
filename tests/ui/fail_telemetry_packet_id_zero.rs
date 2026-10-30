// TelemetryPacket with non-positive packet_id
fn main() -> i32 {
    let packet = TelemetryPacket::new(0, 100, 1, 2, 3, 4);
    0
}
