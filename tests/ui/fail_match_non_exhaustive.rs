// Violation: Non-exhaustive patterns in match
enum Color {
    Red,
    Green,
    Blue,
}

fn test_color(c: Color) -> i32 {
    match c {
        Color::Red => 1,
        Color::Green => 2,
    }
}

fn main() -> i32 {
    test_color(Color::Red)
}
