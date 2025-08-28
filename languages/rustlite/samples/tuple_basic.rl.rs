// Demonstrates tuple construction and element access (Phase A EDN-0011)
fn main() {
    // Target lowering creates an internal __Tuple3 struct; field types currently assumed i32.
    let a = 10;
    let b = 20;
    let c = 30;
    // Rustlite surface (macro) form would be: (tuple %t [ %a %b %c ]) then (tget %x i32 %t 1)
    // Source-style illustrative equivalent:
    let t = (a, b, c);      // conceptual tuple
    let mid = t.1;          // access element index 1
    let _ = mid;            // suppress unused warning analogue
}
