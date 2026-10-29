struct Point {
    x: i32,
    y: i32,
}

impl Point {
    fn new(x: i32, y: i32) -> Point {
        Point { x: x, y: y }
    }
}

fn main() -> i32 {
    let p = Point::new(1, 2);
    p.non_existent();
    0
}
