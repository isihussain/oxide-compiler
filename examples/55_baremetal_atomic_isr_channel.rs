// Example 55: Lock-Free Zero-Allocation ISR Atomic Channel
// Safe, lock-free communication between hardware interrupt service routines and flight control tasks.
// Inspired by lock-free inter-thread and interrupt IPC channels in RT-Thread (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize lock-free AtomicChannel:
    let mut channel = AtomicChannel::new();
    assert!(!channel.has_pending());
    assert!(channel.overrun_count() == 0);
    assert!(channel.sequence_number() == 0);
    assert!(channel.read_count() == 0);

    // 2. Reading from empty channel returns -1:
    let empty_read = channel.receive();
    assert!(empty_read == -1);
    assert!(channel.read_count() == 0);

    // 3. Hardware ISR fires and posts gyro angular rate reading (1050 = 10.50 deg/s):
    let sent1 = channel.send(1050);
    assert!(sent1);
    assert!(channel.has_pending());
    assert!(channel.sequence_number() == 1);
    assert!(channel.overrun_count() == 0);

    // 4. Flight control task reads pending data:
    let rate1 = channel.receive();
    assert!(rate1 == 1050);
    assert!(!channel.has_pending());
    assert!(channel.read_count() == 1);

    // 5. Hardware ISR overrun detection (burst sampling faster than task consumption):
    // First interrupt posts sensor packet 2100:
    let sent2 = channel.send(2100);
    assert!(sent2);
    assert!(channel.has_pending());
    assert!(channel.sequence_number() == 2);
    assert!(channel.overrun_count() == 0);

    // Second interrupt arrives before task can read; overwrites with newest reading 2150:
    let sent3 = channel.send(2150);
    assert!(sent3);
    assert!(channel.has_pending());
    assert!(channel.sequence_number() == 3);
    // Overrun flag tripped and counter incremented:
    assert!(channel.overrun_count() == 1);

    // Flight task receives newest reading:
    let rate2 = channel.receive();
    assert!(rate2 == 2150);
    assert!(!channel.has_pending());
    assert!(channel.read_count() == 2);
    assert!(channel.overrun_count() == 1);

    // 6. Reset verification:
    let reset_ok = channel.reset();
    assert!(reset_ok);
    assert!(!channel.has_pending());
    assert!(channel.overrun_count() == 0);
    assert!(channel.sequence_number() == 0);
    assert!(channel.read_count() == 0);

    42
}
