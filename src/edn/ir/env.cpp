#include "edn/ir/context.hpp"
#include <cstdlib>
#include <string>
#include <algorithm>

namespace edn {

// Reads process env vars and constructs an EmitEnv.
// Note: This is a standalone helper; Context::setEnv stores it for later use.
EmitEnv detectEnv(){
    EmitEnv e{};
    auto get = [](const char* k)->const char*{ const char* v = std::getenv(k); return (v && *v) ? v : nullptr; };

    // Debug info
    if (const char* v = get("EDN_ENABLE_DEBUG")) e.enableDebugInfo = (std::string(v) == "1");

    // Coroutines
    if (const char* v = get("EDN_ENABLE_CORO")) e.enableCoro = (std::string(v) == "1");

    // Panic mode
    if (const char* v = get("EDN_PANIC")) e.panicUnwind = (std::string(v) == "unwind");

    // EH model + enable flag
    std::string model;
    if (const char* v = get("EDN_EH_MODEL")) { model = v; std::transform(model.begin(), model.end(), model.begin(), [](unsigned char c){ return (char)std::tolower(c); }); }
    bool enableEH = false; if (const char* v = get("EDN_ENABLE_EH")) enableEH = (std::string(v) == "1");
    if (!model.empty()){
        // Personality choice mirrors model selection regardless of emission gating
        if (model == "itanium") { e.personalityItanium = true; if (enableEH) e.enableEHItanium = true; }
        else if (model == "seh") { e.personalitySEH = true; if (enableEH) e.enableEHSEH = true; }
    }

    // Target triple (optional)
    if (const char* v = get("EDN_TARGET_TRIPLE")) e.targetTriple = v;

    return e;
}

} // namespace edn
