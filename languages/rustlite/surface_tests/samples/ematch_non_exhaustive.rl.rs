// ematch_non_exhaustive.rl.rs
// Missing one arm for pretend enum MyEnum { A, B }
let v = MyEnum::A;
let r = ematch MyEnum v A { 1 };
