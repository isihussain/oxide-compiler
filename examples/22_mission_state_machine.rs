// Positive test: Verifiable mission flight state machine sequencing and validation
state_machine FlightSequence {
    state PreLaunch;
    state Ignition;
    state Liftoff;
    state MainEngineCutOff;
    terminal Orbit;
    terminal Aborted;

    transition PreLaunch => Ignition;
    transition Ignition => Liftoff;
    transition Ignition => Aborted;
    transition Liftoff => MainEngineCutOff;
    transition Liftoff => Aborted;
    transition MainEngineCutOff => Orbit;
    transition MainEngineCutOff => Aborted;
}

fn sequence_code(seq: FlightSequence) -> i32 {
    match seq {
        FlightSequence::PreLaunch => 1,
        FlightSequence::Ignition => 2,
        FlightSequence::Liftoff => 3,
        FlightSequence::MainEngineCutOff => 4,
        FlightSequence::Orbit => 42,
        FlightSequence::Aborted => 0,
    }
}

fn main() -> i32 {
    let mut seq = FlightSequence::PreLaunch;

    // Verify static transition matrix query
    assert!(FlightSequence::can_transition(seq, FlightSequence::Ignition));
    assert!(!FlightSequence::can_transition(seq, FlightSequence::Orbit));

    // Execute valid flight sequence transitions
    seq = FlightSequence::transition(seq, FlightSequence::Ignition);
    seq = FlightSequence::transition(seq, FlightSequence::Liftoff);
    seq = FlightSequence::transition(seq, FlightSequence::MainEngineCutOff);
    seq = FlightSequence::transition(seq, FlightSequence::Orbit);

    // Verify direct equality check
    assert!(seq == FlightSequence::Orbit);
    assert!(seq != FlightSequence::Aborted);

    sequence_code(seq)
}
