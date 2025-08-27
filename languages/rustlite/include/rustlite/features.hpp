#pragma once
#include <cstdlib>
#include <string_view>

namespace rustlite {
inline bool flag_enabled(const char* name) {
    const char* v = std::getenv(name);
    if(!v) return false;
    return *v=='1' || *v=='t' || *v=='T' || *v=='y' || *v=='Y';
}
inline bool bounds_checks_enabled(){ return flag_enabled("RUSTLITE_BOUNDS"); }
inline bool infer_captures_enabled(){ return flag_enabled("RUSTLITE_INFER_CAPS"); }
}
