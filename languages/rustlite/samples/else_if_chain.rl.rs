// Else-if chain
fn a() {}
fn b() {}
fn c() {}
fn d() {}

fn main() {
    if 0 { a(); }
    else if 1 { b(); }
    else if 0 { c(); }
    else { d(); }
}
