// Nested blocks stress
fn foo(_x: i32) {}
fn bar() {}

fn main() { { let x = 1; { foo(2); } { if 0 {} else { bar(); } } } }
