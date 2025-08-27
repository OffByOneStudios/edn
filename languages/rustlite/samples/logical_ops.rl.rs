fn a() {}
fn b() {}

fn main() {
    if true && false { a(); }
    if (1 < 2) || (3 > 4) { b(); }
}
