// Lightweight utilities for Rustlite test/driver code.
#pragma once
#include <string>

namespace rustlite {
// Monotonic counter to generate unique synthetic module IDs to reduce accidental
// collisions when multiple drivers concatenate or when tests run in parallel.
inline std::string unique_module_id(const std::string& base){
    static unsigned long long counter = 0ULL;
    return base + "_" + std::to_string(++counter);
}
}
