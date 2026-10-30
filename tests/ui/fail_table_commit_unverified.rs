// Error: committing flight table without verifying staging checksum or bounds
fn main() -> i32 {
    let mut table = FlightTable::new(100);
    table.load_staging(500);
    table.commit();
    0
}
