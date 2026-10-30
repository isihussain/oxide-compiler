// Negative test: non-terminal state with no outgoing transitions (deadlock)
state_machine Sequence {
    state Start;
    state Stuck;
    terminal End;

    transition Start => Stuck;
}

fn main() -> i32 {
    0
}
