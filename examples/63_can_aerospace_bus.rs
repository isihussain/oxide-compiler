// ARINC 825 / CAN Aerospace Flight Bus Framer & Bandwidth Schedulability
// Validates message priority slicing, emergency channel ID bounds (< 256),
// payload byte limits (0..8 DLC), and bus bandwidth utilization (<= 70% under ISO 11898).

fn main() -> i32 {
    let mut bus = CanAerospaceBus::new(10, 500, 70);

    if bus.node_id() != 10 {
        return 1;
    }

    if bus.baud_rate_kbps() != 500 {
        return 2;
    }

    if bus.max_bus_load_pct() != 70 {
        return 3;
    }

    // Emergency flight abort frame (channel 0 requires msg_id < 256)
    let ok_emerg = bus.transmit_frame(64, 0, 4, 101);
    if !ok_emerg {
        return 4;
    }

    // High-duty flight guidance frame (channel 1, DLC 8)
    let ok_guidance = bus.transmit_frame(300, 1, 8, 202);
    if !ok_guidance {
        return 5;
    }

    // Low-duty housekeeping telemetry frame (channel 2, DLC 8)
    let ok_telemetry = bus.transmit_frame(500, 2, 8, 303);
    if !ok_telemetry {
        return 6;
    }

    if bus.frames_sent() != 3 {
        return 7;
    }

    // Bus saturation checks
    if bus.is_bus_saturated(55) {
        return 8;
    }

    if !bus.is_bus_saturated(75) {
        return 9;
    }

    // Reset metrics
    bus.reset();
    if bus.frames_sent() != 0 {
        return 10;
    }

    42
}
