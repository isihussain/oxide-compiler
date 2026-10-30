// Negative test: terminal state cannot declare outgoing transitions
state_machine Sequence {
    state Start;
    terminal End;

    transition Start => End;
    transition End => Start;
}

fn main() -> i32 {
    0
}
