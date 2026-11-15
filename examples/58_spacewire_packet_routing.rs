// Test 58: SpaceWire (ECSS-E-ST-50-12C) Packet Router & Link Recovery (SpaceWireRouter)
// Verifies logical address routing, packet header construction, link error handling,
// and state-machine link recovery under --profile=aerospace.

fn main() -> i32 {
    // Initialize SpaceWire router for logical node address 34 at 100 Mbps
    let mut router = SpaceWireRouter::new(34, 100);
    assert!(router.node_address() == 34);
    assert!(router.link_speed_mbps() == 100);
    assert!(router.link_state() == 6); // 6 = Run state
    assert!(router.error_count() == 0);
    assert!(router.routed_count() == 0);
    assert!(router.reconnect_count() == 0);

    // Route packet to instrument payload node 50 with protocol ID 1 (256 bytes)
    let hdr1 = router.route_packet(50, 1, 256);
    assert!(hdr1 > 0);
    assert!(router.routed_count() == 1);

    // Route telemetry packet to central recorder node 80 with protocol ID 2 (512 bytes)
    let hdr2 = router.route_packet(80, 2, 512);
    assert!(hdr2 > 0);
    assert!(router.routed_count() == 2);

    // Simulate transient parity / disconnect link error
    let state_err = router.report_link_error(1);
    assert!(state_err == 1); // 1 = ErrorReset
    assert!(router.link_state() == 1);
    assert!(router.error_count() == 1);

    // While link is in ErrorReset, packets must be dropped
    let dropped = router.route_packet(50, 1, 128);
    assert!(dropped == -1);
    assert!(router.routed_count() == 2); // Count unchanged

    // Autonomous link recovery back to Run state
    let rec_ok = router.reconnect();
    assert!(rec_ok);
    assert!(router.link_state() == 6);
    assert!(router.reconnect_count() == 1);

    // Transmission resumes after link recovery
    let hdr3 = router.route_packet(50, 1, 128);
    assert!(hdr3 > 0);
    assert!(router.routed_count() == 3);

    // Reset router counters and state
    let reset_ok = router.reset();
    assert!(reset_ok);
    assert!(router.link_state() == 6);
    assert!(router.error_count() == 0);
    assert!(router.routed_count() == 0);
    assert!(router.reconnect_count() == 0);

    42
}
