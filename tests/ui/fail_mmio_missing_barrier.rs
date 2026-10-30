// Negative test: consecutive actuator writes without a sequence barrier in aerospace profile
fn main() -> i32 {
    let mut igniter: VolatileRegister<i32, WriteOnly> = VolatileRegister::new(0x40001000);
    let mut valve: VolatileRegister<i32, WriteOnly> = VolatileRegister::new(0x40001004);

    igniter.write(1);
    valve.write(1);

    0
}
