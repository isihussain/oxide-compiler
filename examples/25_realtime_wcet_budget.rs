// Example 25: Real-Time WCET (Worst-Case Execution Time) Loop Budgeting
//
// Hard real-time flight controllers (e.g. SpaceX Falcon 9 and Starship 100 Hz GNC loops)
// strictly require that each periodic guidance update completes within an allocated
// microsecond budget (e.g. 10 ms = 10,000 us).
//
// Under --profile=aerospace, the Oxide compiler performs static path analysis on the CFG
// to compute the worst-case cycle count and verifies that all execution paths finish
// before the hard real-time deadline.

#[realtime(budget_us = 10000)]
fn gnc_attitude_update(pitch_error: i32, yaw_error: i32) -> i32 {
    let mut pitch_cmd = 0;
    let mut yaw_cmd = 0;

    // Simulated 10-iteration Kalman filter state projection
    for step in 0..10 {
        pitch_cmd = pitch_cmd + (pitch_error * 2) + step;
        yaw_cmd = yaw_cmd + (yaw_error * 3) - step;
    }

    if pitch_cmd > 100 {
        pitch_cmd = 100;
    } else {
        pitch_cmd = pitch_cmd + 5;
    }

    pitch_cmd + yaw_cmd
}

#[realtime(budget_us = 5000)]
fn thruster_quaternion_slew() -> i32 {
    let q0 = 1;
    let q1 = 0;
    let q2 = 0;
    let q3 = 0;

    let norm_sq = (q0 * q0) + (q1 * q1) + (q2 * q2) + (q3 * q3);
    assert!(norm_sq == 1);
    norm_sq
}

fn main() -> i32 {
    let gnc_res = gnc_attitude_update(2, 1);
    let slew_res = thruster_quaternion_slew();

    println!("GNC attitude update and quaternion slew verified within real-time budgets.");

    // Exit code: 42
    if gnc_res > 0 {
        if slew_res == 1 {
            42
        } else {
            0
        }
    } else {
        0
    }
}
