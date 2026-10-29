fn modify(val: &i32) {
    *val = 10;
}

fn main() -> i32 {
    let x: i32 = 5;
    modify(&x);
    x
}
