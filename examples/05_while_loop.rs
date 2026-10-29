fn sum_to(n: i32) -> i32 {
    let mut sum: i32 = 0;
    let mut i: i32 = 1;
    while i <= n {
        sum = sum + i;
        i = i + 1;
    }
    sum
}

fn main() -> i32 {
    sum_to(10)
}
