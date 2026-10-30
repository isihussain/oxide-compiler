// Error: calling mutating load_staging on immutable table
fn main() -> i32 {
    let table = FlightTable::new(100);
    table.load_staging(200);
    0
}
