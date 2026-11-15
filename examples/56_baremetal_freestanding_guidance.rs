// Example 56: Freestanding Bare-Metal Flight Avionics Guidance Controller
// Pure ISO C99 freestanding execution without OS, heap allocator, or POSIX C runtime.
// Built with --target=baremetal --profile=aerospace.
// Inspired by bare-metal BSP and linker script architectures in RT-Thread (https://github.com/RT-Thread/rt-thread).

fn main() -> i32 {
    // 1. Initialize static Rate Monotonic Scheduler for bare-metal flight tasks:
    // Task 1: Attitude determination (period: 10 ms, WCET: 2 ms -> 20% CPU)
    // Task 2: Guidance trajectory calculation (period: 20 ms, WCET: 5 ms -> 25% CPU)
    // Task 3: Actuator servo command dispatch (period: 50 ms, WCET: 5 ms -> 10% CPU)
    // Total utilization = 55% <= RMS bound for 3 tasks (77.97%)
    let mut sched = RmsScheduler::new(3);
    assert!(sched.add_task(1, 10000, 2000));
    assert!(sched.add_task(2, 20000, 5000));
    assert!(sched.add_task(3, 50000, 5000));
    assert!(sched.is_schedulable());
    assert!(!sched.is_overloaded());
    assert!(sched.total_utilization_basis_points() == 5500);

    // RMS assigns highest priority to shortest period (Task 1 > Task 2 > Task 3):
    let p1 = sched.assigned_priority(1);
    let p2 = sched.assigned_priority(2);
    let p3 = sched.assigned_priority(3);
    assert!(p1 > p2);
    assert!(p2 > p3);

    // 2. Lock-free AtomicChannel transferring rate gyro reading from hardware ISR:
    let mut gyro_channel = AtomicChannel::new();
    assert!(!gyro_channel.has_pending());

    // Hardware ISR posts angular rate reading: 1200 millirad/s
    assert!(gyro_channel.send(1200));
    assert!(gyro_channel.has_pending());
    assert!(gyro_channel.sequence_number() == 1);

    // Flight guidance task consumes sensor reading:
    let rate = gyro_channel.receive();
    assert!(rate == 1200);
    assert!(!gyro_channel.has_pending());

    // 3. Compute fixed-point steering deflection command via Q16.16:
    // Scale rate down by factor of 30: 1200 / 30 = 40 millirad
    let rate_q = Q16_16::from_int(rate);
    let divisor_q = Q16_16::from_int(30);
    let cmd_q = rate_q / divisor_q;
    let deflection = cmd_q.to_int();
    assert!(deflection == 40);

    // 4. Memory-mapped I/O (MMIO) register update with hardware memory barrier:
    // Gimbal servo command register at address 0x40010000
    let mut servo_reg: VolatileRegister<i32, ReadWrite> = VolatileRegister::new(0x40010000, 0);
    servo_reg.write(deflection);
    memory_barrier();

    // Verify servo register output and add confirmation offset:
    let final_cmd = servo_reg.read();
    assert!(final_cmd == 40);

    final_cmd + 2
}
