// ARINC 429 Dual-Wire Avionics Bus Protocol
// Validates 32-bit word framing, label encoding, SDI, 19-bit data payload,
// Sign/Status Matrix (SSM), and odd parity generation and checking under --profile=aerospace.

fn main() -> i32 {
    let mut channel = Arinc429Channel::new(1, 100);

    // Label 0o203 (131 decimal: Barometric Altitude)
    // SDI = 1 (Air Data Computer 1)
    // Altitude = 35000 feet (19-bit BNR data)
    // SSM = 3 (Normal Operation)
    let alt_label = 131;
    let sdi = 1;
    let alt_data = 35000;
    let ssm_norm = 3;

    let word1 = channel.pack_word(alt_label, sdi, alt_data, ssm_norm);

    // Check validity and field extraction
    if !channel.is_valid(word1) {
        return 1;
    }

    if channel.get_label(word1) != 131 {
        return 2;
    }

    if channel.get_sdi(word1) != 1 {
        return 3;
    }

    if channel.get_ssm(word1) != 3 {
        return 4;
    }

    let unpacked_alt = channel.unpack_word(word1);
    if unpacked_alt != 35000 {
        return 5;
    }

    // Label 0o310 (200 decimal: Inertial Pitch Angle)
    // SDI = 2 (Inertial Reference Unit 2)
    // Pitch = 12500 (fractional pitch in BNR)
    // SSM = 3 (Normal Operation)
    let pitch_label = 200;
    let pitch_sdi = 2;
    let pitch_data = 12500;

    let word2 = channel.pack_word(pitch_label, pitch_sdi, pitch_data, ssm_norm);

    if !channel.is_valid(word2) {
        return 6;
    }

    let unpacked_pitch = channel.unpack_word(word2);
    if unpacked_pitch != 12500 {
        return 7;
    }

    if channel.words_transmitted() != 2 {
        return 8;
    }

    42
}
