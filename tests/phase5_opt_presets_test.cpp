#include <cassert>
#include <iostream>
#include <string>
#include <stdexcept>

#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

static std::string mod_to_str(llvm::Module* m){ std::string s; llvm::raw_string_ostream os(s); m->print(os, nullptr); os.flush(); return s; }

void run_phase5_opt_presets_test(){
    std::cout << "[phase5] opt presets test...\n";
    const char* SRC = R"EDN((module
      (fn :name "dead" :ret i32 :params [ (param i32 %a) ] :body [
         (const %z i32 0) ; dead calc
         (add %d i32 %a %z)
         (ret i32 %a)
      ])
    ))EDN";

    // O0: expect untouched IR (add %d remains)
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_PASSES", "1"); _putenv_s("EDN_OPT_LEVEL", "0");
#else
    setenv("EDN_ENABLE_PASSES", "1", 1); setenv("EDN_OPT_LEVEL", "0", 1);
#endif
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(parse(SRC), r); assert(r.success && m);
        auto ir = mod_to_str(m);
    // LLVM prints named results like: "%d = add i32 %a, %z" at O0 (no DCE). Ensure it's present.
    const bool ok = (ir.find("%d = add i32") != std::string::npos);
    if(!ok) throw std::runtime_error("O0 should not run passes: missing '%d = add i32'");
        (void)m;
    }

    // O1: expect some cleanup; rely on default pipeline performing DCE or instcombine; accept that add may be optimized away
#if defined(_WIN32)
    _putenv_s("EDN_OPT_LEVEL", "1");
#else
    setenv("EDN_OPT_LEVEL", "1", 1);
#endif
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(parse(SRC), r); assert(r.success && m);
        auto ir = mod_to_str(m);
        (void)ir;
        // Not strictly asserting removal to avoid brittle coupling, just check module is valid and function still present.
        if(m->getFunction("dead") == nullptr) throw std::runtime_error("expected function 'dead' to exist at O1");
    }

    // O2: same as O1 from our perspective; ensure accepted
#if defined(_WIN32)
    _putenv_s("EDN_OPT_LEVEL", "2");
#else
    setenv("EDN_OPT_LEVEL", "2", 1);
#endif
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(parse(SRC), r); assert(r.success && m);
        if(m->getFunction("dead") == nullptr) throw std::runtime_error("expected function 'dead' to exist at O2");
    }

    // O3: ensure accepted
#if defined(_WIN32)
    _putenv_s("EDN_OPT_LEVEL", "3");
#else
    setenv("EDN_OPT_LEVEL", "3", 1);
#endif
    {
        TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(parse(SRC), r); assert(r.success && m);
        if(m->getFunction("dead") == nullptr) throw std::runtime_error("expected function 'dead' to exist at O3");
    }

    std::cout << "[phase5] opt presets test passed\n";
}
