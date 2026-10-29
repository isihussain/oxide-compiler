enum Option {
    Some(i32),
    None,
}

enum Result {
    Ok(i32),
    Err(i32),
}

fn checked_divide(numerator: i32, denominator: i32) -> Result {
    if denominator == 0 {
        Result::Err(1)
    } else {
        Result::Ok(numerator / denominator)
    }
}

fn find_value(flag: bool) -> Option {
    if flag {
        Option::Some(30)
    } else {
        Option::None
    }
}

fn main() -> i32 {
    let opt = find_value(true);
    let opt_none = find_value(false);

    assert!(opt.is_some());
    assert!(opt_none.is_none());

    let val = opt.unwrap();

    let res = checked_divide(24, 2);
    assert!(res.is_ok());

    let res_err = checked_divide(10, 0);
    assert!(res_err.is_err());

    let div_val = res.unwrap();

    val + div_val
}
