// Example 32: Double-Buffered Flight Parameter Table (NASA cFS TBL Architecture)
// Demonstrates in-flight parameter table updates with active/inactive staging buffers,
// checksum verification, bounded validation, atomic pointer commit, and non-destructive rollback.

fn main() -> i32 {
    // Initialize flight parameter table with baseline thrust gain (100)
    let mut table = FlightTable::new(100);
    assert!(table.active() == 100);
    assert!(table.commit_count() == 0);
    assert!(!table.is_staging_loaded());

    // Uplink new flight control parameter to inactive staging buffer
    table.load_staging(250);
    assert!(table.staging() == 250);
    assert!(table.is_staging_loaded());
    assert!(!table.is_staging_valid());

    // Verify staging checksum prior to activation
    let expected_crc = FlightTable::compute_checksum(250);
    assert!(table.verify_staging(expected_crc));
    assert!(table.is_staging_valid());

    // Execute atomic buffer commit
    let committed = table.commit();
    assert!(committed);
    assert!(table.active() == 250);
    assert!(table.commit_count() == 1);
    assert!(!table.is_staging_loaded());

    // Fault injection & rollback: upload corrupt parameter, discard via rollback
    table.load_staging(999);
    assert!(table.is_staging_loaded());
    table.rollback();
    assert!(!table.is_staging_loaded());
    assert!(table.active() == 250); // Active parameters preserved unmodified

    // Update with bounds validation: validate parameter within physical envelope [0, 100]
    table.load_staging(42);
    let valid_envelope = table.validate_bounds(0, 100);
    assert!(valid_envelope);
    assert!(table.is_staging_valid());

    // Atomic commit to active buffer
    let new_param = table.commit_or_abort();
    assert!(new_param == 42);
    assert!(table.active() == 42);
    assert!(table.commit_count() == 2);

    println!("NASA cFS FlightTable verification successful: active parameter = {}", table.active());
    table.active()
}
