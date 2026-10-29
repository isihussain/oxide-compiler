struct Counter {
    val: i32,
}

impl Counter {
    fn recurse(&self) -> i32 {
        self.recurse()
    }
}

fn main() -> i32 {
    let c = Counter { val: 0 };
    c.recurse()
}
