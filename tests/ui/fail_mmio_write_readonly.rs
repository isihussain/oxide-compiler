// Negative test: write operation attempted on a ReadOnly register
fn main() -> i32 {
    let mut sensor: VolatileRegister<i32, ReadOnly> = VolatileRegister::new(0x40001000, 42);
    sensor.write(100);
    0
}
