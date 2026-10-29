fn sum_array(arr: &[i32; 4]) -> i32 {
    let mut sum = 0;
    let mut i = 0;
    while i < 4 {
        sum = sum + arr[i];
        i = i + 1;
    }
    sum
}

fn main() -> i32 {
    let mut a: [i32; 4] = [10, 20, 5, 7];
    a[2] = 12;
    let total = sum_array(&a);

    let repeated: [i32; 3] = [7; 3];
    let rep_sum = repeated[0] + repeated[1] + repeated[2];

    total - rep_sum + 14
}
