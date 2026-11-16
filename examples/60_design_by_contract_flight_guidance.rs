// Design-by-Contract Flight Guidance Verification
// Validates #[requires] preconditions and #[ensures] postconditions with static range bounds
// Inspired by SPARK Ada behavioral contracts and NASA JPL formal methods

#[requires(thrust_kn >= 100 && thrust_kn <= 1200)]
#[requires(burn_sec > 0 && burn_sec <= 300)]
#[ensures(result >= 0 && result <= 360000)]
fn calculate_total_impulse(thrust_kn: i32, burn_sec: i32) -> i32 {
    thrust_kn * burn_sec
}

#[requires(apogee_km >= 150 && apogee_km <= 2000)]
#[requires(perigee_km >= 150 && perigee_km <= 2000)]
#[ensures(result >= 0)]
fn calculate_semi_major_axis_eccentricity(apogee_km: i32, perigee_km: i32) -> i32 {
    (apogee_km - perigee_km) * 1000 / (apogee_km + perigee_km)
}

#[requires(pitch_deg >= -45 && pitch_deg <= 45)]
#[ensures(result >= 0 && result <= 90)]
fn normalize_attitude_pitch(pitch_deg: i32) -> i32 {
    pitch_deg + 45
}

fn main() -> i32 {
    let impulse = calculate_total_impulse(500, 100);
    assert!(impulse == 50000);

    let ecc = calculate_semi_major_axis_eccentricity(500, 400);
    assert!(ecc == 111);

    let norm_pitch = normalize_attitude_pitch(-30);
    assert!(norm_pitch == 15);

    42
}
