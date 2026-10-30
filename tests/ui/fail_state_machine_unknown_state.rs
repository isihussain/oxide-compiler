// Negative test: transition references an undeclared state
state_machine Sequence {
    state Start;
    terminal End;

    transition Start => NonExistent;
}

fn main() -> i32 {
    0
}
