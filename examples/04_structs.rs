struct Point {
    x: i32,
    y: i32,
}

fn manhattan_distance(p: Point) -> i32 {
    p.x + p.y
}

fn main() -> i32 {
    let p: Point = Point { x: 15, y: 27 };
    manhattan_distance(p)
}
