fn main() -> i32 {
    let interlock = ColdGasAnchoringInterlock::new(50, 100);
    interlock.evaluate_touchdown(85, 140, true, true);
    0
}
