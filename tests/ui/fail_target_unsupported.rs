// Negative UI test: Reject unsupported compilation target
// Usage: oxide --check --target=wasm32 tests/ui/fail_target_unsupported.rs
// Expected error: unsupported target 'wasm32'. Supported targets: baremetal, hosted

fn main() -> i32 {
    0
}
