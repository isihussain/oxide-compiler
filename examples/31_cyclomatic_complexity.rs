// Example 31: McCabe Cyclomatic Complexity Verification (NASA NESC / SLS Standard)
//
// In human-rated and safety-critical flight software (NASA SLS, Orion, NESC standards),
// code units with excessive cyclomatic complexity (M = E - N + 2P) are prohibited
// because exponential test path explosion prevents exhaustive Modified Condition / Decision Coverage (MC/DC).
// This example verifies that flight control routines adhere to bounded cyclomatic complexity constraints.

#[max_complexity = 5]
fn flight_abort_evaluator(dynamic_pressure: i32, structural_load: i32) -> i32 {
    let mut code = 0;
    if dynamic_pressure > 35000 {
        code = 1;
    }
    if structural_load > 85000 {
        code = code + 2;
    }
    code
}

fn main() -> i32 {
    let status1 = flight_abort_evaluator(20000, 50000);
    if status1 != 0 {
        return 1;
    }

    let status2 = flight_abort_evaluator(40000, 50000);
    if status2 != 1 {
        return 2;
    }

    let status3 = flight_abort_evaluator(40000, 90000);
    if status3 != 3 {
        return 3;
    }

    println!("NASA cyclomatic complexity constraints verified successfully.");
    42
}
