struct Counter {
    val: i32,
}

fn main() -> i32 {
    let mut c: Counter = Counter { val: 10 };
    c.val = c.val + 20;
    c.val = c.val + 12;
    c.val
}
