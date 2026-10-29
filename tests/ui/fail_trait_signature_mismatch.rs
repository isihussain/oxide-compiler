struct Rectangle {
    width: i32,
    height: i32,
}

trait Area {
    fn area(&self) -> i32;
}

impl Area for Rectangle {
    fn area(&self) -> bool {
        true
    }
}

fn main() -> i32 {
    0
}
