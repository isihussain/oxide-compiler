// Error: invalid flight table bounds with min > max
fn main() -> i32 {
    let mut table = FlightTable::new(100);
    table.load_staging(50);
    table.validate_bounds(200, 100);
    0
}
