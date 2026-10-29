fn print_greeting(msg: &str) -> i32 {
    println!("Message: {}", msg);
    42
}

fn main() -> i32 {
    let greeting: &str = "Oxide Mission Control";
    println!("Launching: {}", greeting);
    let code = print_greeting(greeting);
    assert!(code == 42);
    code
}
