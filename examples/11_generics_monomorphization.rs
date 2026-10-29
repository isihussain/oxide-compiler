// Test parametric polymorphism and monomorphization
struct Container<T> {
    item: T,
}

fn unwrap<T>(c: Container<T>) -> T {
    c.item
}

fn pick_first<T>(a: T, b: T) -> T {
    a
}

fn main() -> i32 {
    let int_box = Container { item: 40 };
    let bool_box = Container { item: true };

    let num = unwrap(int_box);
    let ok = unwrap(bool_box);

    let extra = pick_first(2, 99);

    if ok {
        num + extra
    } else {
        0
    }
}
