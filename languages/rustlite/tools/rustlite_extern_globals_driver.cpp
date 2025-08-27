// Test rextern-global / rextern-const macros and literal interning reuse.
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
    std::cout << "[rustlite-extern-globals] demo...\n";
    // Two identical rcstr literals should intern to one global; test extern global macros expand.
    const char* edn_text =
        "(module :id \"rl_extern_globs\" "
        "  (rextern-global :name EXTG :type i32) "
        "  (rextern-const :name EXTC :type i32) "
        "  (rfn :name \"use\" :ret i32 :params [ ] :body [ "
        "    (rcstr %s1 \"same\") (rcstr %s2 \"same\") "
        "    (const %z i32 0) (ret i32 %z) "
        "  ]) "
        ")";
    auto ast = parse(edn_text);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){ std::cerr << "[rustlite-extern-globals] type check failed\n"; for(auto &e: tcres.errors) std::cerr<<e.code<<": "<<e.message<<"\n"; return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    auto irs = module_to_ir(mod);
    // Expect exactly one __edn.cstr.* for the duplicated string.
    size_t count=0, pos=0; while((pos = irs.find("__edn.cstr.", pos))!=std::string::npos){ ++count; pos += 11; }
    if(count!=1){ std::cerr << "[rustlite-extern-globals] expected 1 interned cstr global, saw "<<count<<"\n"; return 2; }
    // Ensure extern globals present (they'll appear by name EXTG / EXTC without initializer or with external linkage semantics).
    if(irs.find("EXTG")==std::string::npos || irs.find("EXTC")==std::string::npos){ std::cerr << "[rustlite-extern-globals] missing extern symbols\n"; return 3; }
    std::cout << "[rustlite-extern-globals] ok\n";
    return 0;
}
