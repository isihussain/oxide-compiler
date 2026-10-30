// Negative test: states forming a cycle with no reachable terminal state (deadlock trap)
state_machine TrappedSequence {
    state Start;
    state LoopA;
    state LoopB;
    terminal Goal;

    transition Start => LoopA;
    transition LoopA => LoopB;
    transition LoopB => LoopA;
}

fn main() -> i32 {
    0
}
