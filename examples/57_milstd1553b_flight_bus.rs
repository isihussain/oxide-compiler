// Test 57: MIL-STD-1553B Dual-Redundant Flight Bus Framer (MilStd1553Bus)
// Verifies command word encoding, status word reception, retry tracking,
// and automatic channel failover (Bus A to Bus B) under --profile=aerospace.

fn main() -> i32 {
    // Initialize MIL-STD-1553B bus controller on primary Bus A (0) with 3 retries
    let mut bus = MilStd1553Bus::new(0, 3);
    assert!(bus.active_channel() == 0);
    assert!(bus.failover_count() == 0);
    assert!(bus.command_count() == 0);
    assert!(bus.retry_count() == 0);

    // Form and send command word to Remote Terminal 5, Subaddress 2, Word Count 16
    let cmd1 = bus.send_command(5, 2, 16);
    assert!(cmd1 > 0);
    assert!(bus.command_count() == 1);

    // Receive nominal status word (0 errors)
    let ok1 = bus.receive_status(0);
    assert!(ok1);
    assert!(bus.retry_count() == 0);

    // Simulate transient message error (bit 9 = 512 / 0x0200)
    let err1 = bus.receive_status(512);
    assert!(!err1);
    assert!(bus.retry_count() == 1);
    assert!(bus.active_channel() == 0); // No failover on single transient error

    // Recovery on retry
    let ok2 = bus.receive_status(0);
    assert!(ok2);
    assert!(bus.retry_count() == 0);

    // Exceed maximum retries (3 consecutive transmission errors)
    bus.receive_status(512);
    assert!(bus.retry_count() == 1);
    bus.receive_status(512);
    assert!(bus.retry_count() == 2);
    bus.receive_status(512); // Triggers dual-channel failover to Bus B (1)

    assert!(bus.active_channel() == 1);
    assert!(bus.failover_count() == 1);
    assert!(bus.retry_count() == 0);

    // Send flight command across backup Bus B
    let cmd_b = bus.send_command(12, 4, 32);
    assert!(cmd_b > 0);
    assert!(bus.command_count() == 2);

    let ok_b = bus.receive_status(0);
    assert!(ok_b);

    // Manual channel switch back to Bus A
    let switched = bus.switch_channel(0);
    assert!(switched);
    assert!(bus.active_channel() == 0);
    assert!(bus.failover_count() == 2);

    // Reset bus state
    let reset_ok = bus.reset();
    assert!(reset_ok);
    assert!(bus.active_channel() == 0);
    assert!(bus.retry_count() == 0);

    42
}
