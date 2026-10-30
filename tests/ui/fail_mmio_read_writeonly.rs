// Negative test: read operation attempted on a WriteOnly register
fn main() -> i32 {
    let igniter: VolatileRegister<i32, WriteOnly> = VolatileRegister::new(0x40001000);
    let val = igniter.read();
    val
}
