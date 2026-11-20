fn main() -> i32 {
    let channel = Arinc429Channel::new(0, 100);
    channel.pack_word(10, 0, 100, 3);
    0
}
