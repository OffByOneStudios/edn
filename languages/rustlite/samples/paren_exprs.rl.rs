// Parenthesized expressions sample
fn foo(_a: i32, _b: i32) {}

fn main() {
    let mut x = (1);
    x = (x);
    if (x) { }
    while (x) { break; }
    foo((1), (x));
}
