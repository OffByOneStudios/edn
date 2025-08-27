// enum_basic.rl.rs - simple enum-like pattern using stubs until full enum parsing.
// When surface enum syntax is supported, replace this with:
//   enum OptionI32 { Some(i32), None }
//   fn main() { let v = OptionI32::Some(3); /* match */ }
// For now just show functions standing in for variants.

fn some(val: i32) -> i32 { val }
fn none() -> i32 { 0 }

fn main() {
	let v = some(3);
	if v != 0 { /* pretend Some */ } else { /* pretend None */ }
}

