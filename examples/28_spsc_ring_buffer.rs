// Example 28: Lock-Free Zero-Allocation SPSC Telemetry Ring Buffer
//
// In hard real-time rocketry avionics, sensor sampling threads (such as 1 kHz IMUs,
// chamber pressure transducers, and GPS receivers) cannot take locks (mutexes)
// or allocate heap memory. Doing so risks priority inversion, thread jitter, and memory fragmentation.
// This example verifies a lock-free Single-Producer Single-Consumer circular queue
// using atomic acquire/release memory barriers and fixed static storage.

fn main() -> i32 {
    // 1. Initialize SPSC ring buffer with capacity 4 (fixed static storage, zero heap)
    let mut telemetry_queue = SpscRingBuffer::new(4);

    if !telemetry_queue.is_empty() {
        return 1;
    }
    if telemetry_queue.len() != 0 {
        return 2;
    }
    if telemetry_queue.capacity() != 4 {
        return 3;
    }

    // 2. Producer enqueues 4 high-frequency telemetry samples (chamber pressure in kPa)
    let p1 = telemetry_queue.push(1013);
    let p2 = telemetry_queue.push(1020);
    let p3 = telemetry_queue.push(1035);
    let p4 = telemetry_queue.push(1050);

    if !p1 { return 4; }
    if !p2 { return 5; }
    if !p3 { return 6; }
    if !p4 { return 7; }

    if !telemetry_queue.is_full() {
        return 8;
    }
    if telemetry_queue.len() != 4 {
        return 9;
    }

    // 3. Buffer is full: non-blocking backpressure gracefully drops or rejects sample #5
    let p5 = telemetry_queue.push(1060);
    if p5 {
        return 10;
    }
    println!("SPSC backpressure on full buffer: NOMINAL");

    // 4. Consumer drains 2 samples via acquire barrier
    let v1 = telemetry_queue.pop();
    let v2 = telemetry_queue.pop();
    if v1 != 1013 { return 11; }
    if v2 != 1020 { return 12; }
    if telemetry_queue.len() != 2 { return 13; }
    if telemetry_queue.is_full() { return 14; }

    // 5. Producer wraps around circular buffer without allocation or locks
    let p6 = telemetry_queue.push(1060);
    let p7 = telemetry_queue.push(1075);
    if !p6 { return 15; }
    if !p7 { return 16; }
    if !telemetry_queue.is_full() { return 17; }

    // 6. Consumer drains remaining 4 samples in FIFO order
    let v3 = telemetry_queue.pop();
    let v4 = telemetry_queue.pop();
    let v5 = telemetry_queue.pop();
    let v6 = telemetry_queue.pop();

    if v3 != 1035 { return 18; }
    if v4 != 1050 { return 19; }
    if v5 != 1060 { return 20; }
    if v6 != 1075 { return 21; }

    if !telemetry_queue.is_empty() { return 22; }
    if telemetry_queue.len() != 0 { return 23; }

    println!("Lock-free zero-allocation SPSC telemetry ring buffer verified successfully.");
    42
}
