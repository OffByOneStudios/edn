// ematch_exhaustive.rl.rs
// Simple enum + ematch exhaustive lowering test (synthetic until enum decl syntax present)
// Pretend enum: MyEnum { A, B }
let v = MyEnum::A;
let r = ematch MyEnum v A { 1 } B { 2 };
