// Trait definitions and impl blocks in Oxide
struct Point {
    x: i32,
    y: i32,
}

struct Rectangle {
    width: i32,
    height: i32,
}

trait Area {
    fn area(&self) -> i32;
}

trait Describable {
    fn describe(&self) -> i32;
}

impl Point {
    fn new(x: i32, y: i32) -> Point {
        Point { x: x, y: y }
    }

    fn get_x(&self) -> i32 {
        self.x
    }

    fn translate(&mut self, dx: i32, dy: i32) {
        self.x = self.x + dx;
        self.y = self.y + dy;
    }
}

impl Area for Rectangle {
    fn area(&self) -> i32 {
        self.width * self.height
    }
}

impl Describable for Point {
    fn describe(&self) -> i32 {
        self.x + self.y
    }
}

fn main() -> i32 {
    let mut p = Point::new(10, 20);
    let orig_x = p.get_x();
    if orig_x != 10 {
        return 1;
    }

    p.translate(5, 7);
    let desc = p.describe();
    if desc != 42 {
        return 2;
    }

    let r = Rectangle { width: 6, height: 7 };
    let a = r.area();
    if a != 42 {
        return 3;
    }

    a
}
