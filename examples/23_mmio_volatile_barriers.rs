// Positive test: Type-safe Memory-Mapped I/O (MMIO) and memory sequence barriers
fn main() -> i32 {
    // 1. Read-only telemetry register (chamber pressure sensor at 0x40001000)
    let pressure_sensor: VolatileRegister<i32, ReadOnly> = VolatileRegister::new(0x40001000, 32);
    let pressure: i32 = pressure_sensor.read();
    assert!(pressure == 32);
    assert!(pressure_sensor.addr() == 0x40001000);

    // 2. Write-only actuator register (stage separation igniter at 0x40002000)
    let mut igniter: VolatileRegister<i32, WriteOnly> = VolatileRegister::new(0x40002000);
    igniter.write(1);

    // 3. Hardware sequence barrier guaranteeing actuator write ordering
    memory_barrier();

    // 4. Read-write register (main engine propellant valve at 0x40003000)
    let mut valve: VolatileRegister<i32, ReadWrite> = VolatileRegister::new(0x40003000, 0);
    valve.write(10);
    let valve_pos: i32 = valve.read();
    assert!(valve_pos == 10);

    // Sequence barrier prior to secondary valve adjustment
    memory_barrier();
    valve.write(10);

    pressure + valve_pos
}
