// Example 30: Bit-Packed Zero-Copy Telemetry Serialization & Hardware CRC Verification
//
// In orbital rocket telemetry and Deep Space communications (CCSDS standards),
// bandwidth constraints require bit-packed packet structures with zero padding gaps,
// zero memory allocations, and hardware CRC-32 integrity verification over noisy RF channels.

#[repr(packed)]
struct NavPayload {
    accel_x: i32,
    accel_y: i32,
    accel_z: i32,
}

fn main() -> i32 {
    let nav = NavPayload {
        accel_x: 10,
        accel_y: 20,
        accel_z: 981,
    };
    if nav.accel_z != 981 {
        return 1;
    }

    // 1. Bit-packed avionic telemetry frame (packet_id=42, timestamp=15000 ms, 4 sensor channels):
    // ch0: Chamber pressure (3000 kPa)
    // ch1: Turbopump RPM (36000 rpm)
    // ch2: Gimbal angle (50 mdeg)
    // ch3: Battery voltage (28400 mV)
    let packet = TelemetryPacket::new(42, 15000, 3000, 36000, 50, 28400);

    if packet.packet_id() != 42 {
        return 2;
    }
    if packet.timestamp() != 15000 {
        return 3;
    }
    if packet.channel(0) != 3000 {
        return 4;
    }
    if packet.channel(1) != 36000 {
        return 5;
    }
    if packet.channel(2) != 50 {
        return 6;
    }
    if packet.channel(3) != 28400 {
        return 7;
    }

    // 2. Hardware CRC-32 calculation over the contiguous 24-byte bit-packed frame:
    let crc = packet.compute_crc();
    println!("Frame CRC-32 checksum computed: {}", crc);

    // 3. Verify frame integrity with matching CRC:
    if !packet.verify_crc(crc) {
        return 8;
    }

    // 4. Invariant assertion trap: verifies matching CRC or aborts process:
    let status = packet.verify_or_abort(crc);
    if status != 0 {
        return 9;
    }

    // 5. Corrupted frame detection (e.g. RF bit-flip during S-band transmission):
    let bad_crc = crc + 1;
    if packet.verify_crc(bad_crc) {
        return 10;
    }

    println!("Bit-packed zero-copy telemetry packet and CRC verification verified successfully.");
    42
}
