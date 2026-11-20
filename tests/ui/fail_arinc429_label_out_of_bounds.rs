fn main() -> i32 {
    let mut channel = Arinc429Channel::new(0, 100);
    channel.pack_word(300, 0, 100, 3);
    0
}
