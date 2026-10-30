// Example 34: Static-Queued Publish/Subscribe Flight Software Bus (NASA cFE Software Bus)
// Demonstrates zero-allocation message routing by message ID, bounded static FIFO queues,
// non-blocking queue overflow protection, and multi-channel pub/sub semantics.

fn main() -> i32 {
    let mut bus = SoftwareBus::new();

    // 1. Subscribe tasks to static message channels (0: GNC, 1: Propulsion, 2: Power)
    let s0 = bus.subscribe(0);
    let s1 = bus.subscribe(1);
    assert!(s0);
    assert!(s1);
    assert!(bus.is_subscribed(0));
    assert!(bus.is_subscribed(1));
    assert!(!bus.is_subscribed(2));

    // 2. Publish and receive GNC flight guidance commands on channel 0
    let pub1 = bus.publish(0, 101);
    let pub2 = bus.publish(0, 102);
    assert!(pub1);
    assert!(pub2);
    assert!(bus.has_pending(0));
    assert!(bus.pending_count(0) == 2);

    let recv1 = bus.receive(0);
    let recv2 = bus.receive(0);
    assert!(recv1 == 101);
    assert!(recv2 == 102);
    assert!(!bus.has_pending(0));

    // 3. Channel queue bounded depth & non-blocking overflow protection (queue depth = 4)
    assert!(bus.publish(1, 10));
    assert!(bus.publish(1, 20));
    assert!(bus.publish(1, 30));
    assert!(bus.publish(1, 40));
    assert!(bus.pending_count(1) == 4);

    // 5th message must be safely dropped without crashing or blocking real-time execution
    let overflow_pub = bus.publish(1, 50);
    assert!(!overflow_pub);
    assert!(bus.dropped_count(1) == 1);
    assert!(bus.pending_count(1) == 4);

    // 4. Drain channel 1 messages
    assert!(bus.receive(1) == 10);
    assert!(bus.receive(1) == 20);
    assert!(bus.pending_count(1) == 2);

    // Clear remaining queue entries
    bus.clear(1);
    assert!(!bus.has_pending(1));
    assert!(bus.pending_count(1) == 0);

    println!("NASA cFE Software Bus verification successful: zero-allocation pub/sub verified");
    42
}
