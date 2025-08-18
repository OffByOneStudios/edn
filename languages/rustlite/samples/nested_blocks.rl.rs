// Nested blocks stress
fn main() {{ let x = 1; { foo(2); } { if 0 {} else { bar(); } } }}
