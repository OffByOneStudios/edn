fn main() -> i32 {
    let mut x: i32 = 0;
    let y: i32 = 3;
    if x < y && y < 10 {
        x = x + y;
    } else if x == 0 || y == 0 {
        x = 1;
    } else {
        x = 2;
    }
    while x < 5 {
        x += 1;
        if x == 3 { continue; }
        if x == 4 { break; }
    }
    let mut z: i32 = 0;
    loop {
        z += 2;
        if z > x { break; }
    }
    return z;
}
