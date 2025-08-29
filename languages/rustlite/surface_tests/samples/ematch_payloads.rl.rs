// ematch_payloads.rl.rs
// Enum pretend: MyEnum2 { A(i32), B(i32,i32) }
let v = MyEnum2::B(1,2);
let r = ematch MyEnum2 v A(x) { x } B(a,b) { a };
