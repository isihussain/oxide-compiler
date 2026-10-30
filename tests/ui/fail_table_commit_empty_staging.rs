// Error: committing flight table without loading staging buffer
fn main() -> i32 {
    let mut table = FlightTable::new(100);
    table.commit();
    0
}
