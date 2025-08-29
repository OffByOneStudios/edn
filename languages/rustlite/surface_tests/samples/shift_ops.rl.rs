// shift_ops.rl.rs
// Test precedence and lowering of << and >> relative to + and *.
// Expression: let a = 1 + 2 << 3 >> 1; ensures left-associative chaining at current tier.
let a = 1 + 2 << 3 >> 1;
