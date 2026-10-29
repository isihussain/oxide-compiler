fn add_ten(val: &mut i32) {
    *val = *val + 10;
}

fn main() -> i32 {
    let mut x: i32 = 32;
    add_ten(&mut x);
    x
}
