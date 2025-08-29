fn __surface() -> i32 {
    let mut x: i32 = 1;
    // <<= expands via rassign-op shl
    x <<= 3;
    // >>= expands via rassign-op ashr (arithmetic right shift)
    x >>= 1;
    x
}
