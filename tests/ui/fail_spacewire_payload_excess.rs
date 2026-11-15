fn main() -> i32 {
    let mut router = SpaceWireRouter::new(42, 50);
    router.route_packet(45, 1, 2048);
    0
}
