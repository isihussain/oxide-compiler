// Error: degrade_threshold must be positive in SensorFDIR
fn main() -> i32 {
    let mut fdir = SensorFDIR::new(-500, 500, 0, 6, 4);
    0
}
