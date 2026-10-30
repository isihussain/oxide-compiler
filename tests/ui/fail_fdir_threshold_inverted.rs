// Error: isolate_threshold must exceed degrade_threshold in SensorFDIR
fn main() -> i32 {
    let mut fdir = SensorFDIR::new(-500, 500, 6, 3, 4);
    0
}
