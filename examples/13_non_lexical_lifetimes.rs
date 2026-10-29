// Milestone 4: Non-Lexical Lifetimes (NLL) Conformance Test

struct Counter {
    val: i32,
}

impl Counter {
    fn add(&mut self, delta: i32) {
        self.val = self.val + delta;
    }
}

fn test_sequential_mut_borrows() -> i32 {
    let mut x = 10;

    let r1 = &mut x;
    *r1 = 20;
    // r1 is never used again: loan expires here under NLL.

    let r2 = &mut x; // Valid in NLL
    *r2 = 30;
    // r2 is never used again: loan expires here under NLL.

    x // 30
}

fn test_mutation_after_shared_borrow() -> i32 {
    let mut y = 100;

    let r = &y;
    let v = *r;
    // r is never used again: loan expires here under NLL.

    y = y + v; // Valid in NLL
    y // 200
}

fn test_branch_nll(cond: bool) -> i32 {
    let mut z = 5;
    if cond {
        let r1 = &mut z;
        *r1 = 10;
    } else {
        let r2 = &mut z;
        *r2 = 20;
    }
    // Both branch loans expire before join under NLL.
    z = z + 2; // Valid in NLL
    z // 12
}

fn test_method_nll() -> i32 {
    let mut c = Counter { val: 5 };
    c.add(10);
    c.add(20);
    c.val // 35
}

fn main() -> i32 {
    let a = test_sequential_mut_borrows(); // 30
    if a != 30 {
        return 1;
    }

    let b = test_mutation_after_shared_borrow(); // 200
    if b != 200 {
        return 2;
    }

    let c = test_branch_nll(true); // 12
    if c != 12 {
        return 3;
    }

    let d = test_method_nll(); // 35
    if d != 35 {
        return 4;
    }

    // 30 + 200 + 12 + 35 = 277
    if a + b + c + d != 277 {
        return 5;
    }

    42
}
