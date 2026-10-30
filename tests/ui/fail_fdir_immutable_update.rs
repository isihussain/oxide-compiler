// Error: calling mutating update on immutable SensorFDIR
fn main() -> i32 {
    let fdir = SensorFDIR::new(-500, 500, 3, 6, 4);
    fdir.update(100);
    0
}
