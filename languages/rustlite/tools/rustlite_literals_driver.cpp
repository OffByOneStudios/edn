#include <iostream>
#include <cassert>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

static std::string module_to_ir(llvm::Module* m){ std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf; }

int main(){
    std::cout << "[rustlite-literals] rcstr / rbytes demo...\n";
    // Build a tiny module exercising rcstr and rbytes; we just check expansion shape & typecheck pass.
    const char* edn =
        "(module :id \"rl_lits\" "
        "  (rfn :name \"use_lits\" :ret i32 :params [ ] :body [ "
    "    (rcstr %hello \"hello\") "
        "    (rbytes %raw [ 1 2 3 ]) "
        "    (const %zero i32 0) (ret i32 %zero) "
        "  ]) "
        ")";

    auto ast = parse(edn);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-literals] type check failed\n";
        for(auto &e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    auto irs = module_to_ir(mod);
    // We expect at least one i8 const for the empty string (NUL) and multiple for 'hello'
    assert(irs.find("hello") != std::string::npos || irs.find("104,101,108,108,111") != std::string::npos);
    std::cout << "[rustlite-literals] ok\n";
    return 0;
}
