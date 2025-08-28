// Demonstrates fixed-size array allocation and indexing (Phase A EDN-0011)
fn main() {
    // Macro forms:
    //   (rarray %arr i32 4)                ; allocate array[4] i32
    //   (const %i2 i32 2)
    //   (const %val i32 77)
    //   (rindex-addr %p i32 %arr %i2)
    //   (store i32 %p %val)
    //   (rindex-load %got i32 %arr %i2)
    // Surface illustrative equivalent:
    let mut arr: [i32;4];   // uninitialized conceptual (Rust would need init; shown for intent)
    let i2 = 2;
    let val = 77;
    // arr[i2] = val;  // write
    // let got = arr[i2];
    // assert(got == val);
}
