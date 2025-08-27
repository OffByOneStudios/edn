#include <iostream>
#include <cassert>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

// This driver documents the current permissive behavior for unknown escapes in rcstr:
// An unknown escape sequence like \q is preserved as the literal character 'q' (after the backslash is removed).
// We build a module with such a string and assert successful type checking and the expected decoded content.
static std::string module_to_ir(llvm::Module* m){ std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf; }

int main(){
    std::cout << "[rustlite-cstr-unknown-escape] documenting permissive unknown escape handling...\n";
    const char* edn =
        "(module :id \"rl_cstr_unk_esc\" "
        "  (rfn :name \"use_lits\" :ret i32 :params [ ] :body [ "
        // Introduce unknown escapes: \\q and \\z (neither recognized) -> expected literal q and z
        "    (rcstr %s1 \"hello\\qworld\\z\") "
        "    (const %zero i32 0) (ret i32 %zero) "
        "  ]) "
        ")";

    auto ast = parse(edn);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-cstr-unknown-escape] unexpected type check failure\n";
        for(auto &e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    auto irs = module_to_ir(mod);
    // We expect the substring "helloqworldz" (with unknown escapes taken literally) to appear in some form.
    if(irs.find("helloqworldz") == std::string::npos){
        std::cerr << "[rustlite-cstr-unknown-escape] did not find expected decoded literal 'helloqworldz' in IR output\n";
        // Dump a small portion for debugging
        std::cerr << irs.substr(0, 400) << "\n";
        return 1;
    }
    std::cout << "[rustlite-cstr-unknown-escape] ok (unknown escapes literalized)\n";
    return 0;
}
