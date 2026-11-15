fn main() -> i32 {
    let router = SpaceWireRouter::new(42, 50);
    router.route_packet(45, 1, 64);
    0
}
