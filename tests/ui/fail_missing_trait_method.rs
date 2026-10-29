struct Rectangle {
    width: i32,
    height: i32,
}

trait Area {
    fn area(&self) -> i32;
}

impl Area for Rectangle {
    // Missing method 'area'
}

fn main() -> i32 {
    0
}
