#[requires(42)]
fn validate_telemetry(packet_id: i32) -> i32 {
    packet_id
}

fn main() {
    validate_telemetry(1);
}
