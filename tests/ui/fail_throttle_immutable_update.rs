fn main() -> i32 {
    let corridor = ThrustThrottleCorridor::new(40, 100, 20);
    corridor.command_throttle(50, 1000);
    0
}
