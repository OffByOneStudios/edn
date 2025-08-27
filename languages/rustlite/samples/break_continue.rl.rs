// Break and continue statements (must appear inside loops)
fn tick(mut n: i32) {
    // Skip even numbers, stop at first multiple of 7
    loop {
        n += 1;
        if n % 2 == 0 { continue; }
        if n % 7 == 0 { break; }
    }
}

fn main() { tick(0); }
