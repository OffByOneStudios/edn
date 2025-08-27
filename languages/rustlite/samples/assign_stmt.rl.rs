// Assignment statement sample (mutable variable updates + call)
fn foo(a: i32, b: i32) { /* side-effect stub */ }

fn main() {
    let mut x = 1;   // must be mutable to reassign
    x = 2;
    x = -3;
    foo(1, x);
}
