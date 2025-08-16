#include "edn/type_check.hpp"
#include <algorithm>
#include <cstdlib>

// Implementation moved from include/edn/type_check.inl to cut rebuild times.
// Locally strip 'inline' so we produce strong out-of-line symbols in this TU.
#ifdef inline
#undef inline
#endif
#define inline /* out-of-line in this TU */
#include "edn/type_check.inl"
#undef inline
