// Triple Modular Redundancy (TMR) for Cosmic Radiation Single-Event Upset (SEU) Mitigation
// Automated compiler-directed triplication and majority voting (COTS Avionics / SpaceX Flight Software Pattern)

#[tmr]
fn compute_steering_correction(sensor_reading: i32, target_heading: i32) -> i32 {
    let diff: i32 = target_heading - sensor_reading;
    diff * 2 + 10
}

fn main() -> i32 {
    // 1. Function-level TMR execution
    // Automatically executes across three channels and majority votes the outcome
    let steering: i32 = compute_steering_correction(100, 110); // diff = 10 -> 10 * 2 + 10 = 30

    // 2. Explicit TMR Voting on simulated cosmic radiation SEU
    // Channel 0 and Channel 1 compute the nominal value 12
    let ch0: i32 = 12;
    let ch1: i32 = 12;
    // Channel 2 suffers a high-energy proton/neutron bit flip in RAM/cache
    let ch2: i32 = 999999;

    // The majority voter detects channel 2 discordance, emits telemetry, and recovers 12
    let recovered_altitude_delta: i32 = tmr_vote(ch0, ch1, ch2);

    let mut status: i32 = 0;
    if steering == 30 {
        status = status + 30;
    }

    if recovered_altitude_delta == 12 {
        status = status + 12;
    }

    // Status: 30 + 12 = 42
    status
}
