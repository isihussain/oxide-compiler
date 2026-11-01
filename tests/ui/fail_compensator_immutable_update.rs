// Error: calling mutating update on immutable EngineOutCompensator
fn main() -> i32 {
    let comp = EngineOutCompensator::new(4, 20, 3000, 150);
    comp.update(15, 50);
    0
}
