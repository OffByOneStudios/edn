// Audit that literal globals produced by cstr/bytes interning are neither duplicated nor
// affected by variable aliasing or nested emission ordering.
#include <iostream>
#include <cassert>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

static std::string module_to_ir(llvm::Module* m){ std::string buf; llvm::raw_string_ostream os(buf); m->print(os,nullptr); os.flush(); return buf; }

int main(){
    std::cout << "[rustlite-literals-alias-audit] running...\n";
    // Multiple identical rcstr/rbytes across nested blocks; ensure single global each.
    const char* edn_text =
        "(module :id \"rl_lits_alias\" "
        "  (rfn :name \"use\" :ret i32 :params [ ] :body [ "
        "     (rcstr %s1 \"alpha\") (rcstr %s2 \"alpha\") "
        "     (rbytes %b1 [ 1 2 3 ]) (rbytes %b2 [ 1 2 3 ]) "
        "     (const %z i32 0) (ret i32 %z) "
        "  ]) "
        ")";
    auto ast = parse(edn_text);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){ std::cerr << "[rustlite-literals-alias-audit] type check failed\n"; for(auto &e: tcres.errors) std::cerr<<e.code<<": "<<e.message<<"\n"; return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    auto irs = module_to_ir(mod);
    size_t cstrCount=0, pos=0; while((pos = irs.find("__edn.cstr.", pos))!=std::string::npos){ ++cstrCount; pos += 11; }
    size_t bytesCount=0; pos=0; while((pos = irs.find("__edn.bytes.", pos))!=std::string::npos){ ++bytesCount; pos += 12; }
    if(cstrCount!=1){ std::cerr << "[rustlite-literals-alias-audit] expected 1 cstr global, saw "<<cstrCount<<"\n"; return 2; }
    if(bytesCount!=1){ std::cerr << "[rustlite-literals-alias-audit] expected 1 bytes global, saw "<<bytesCount<<"\n"; return 3; }
    std::cout << "[rustlite-literals-alias-audit] ok\n";
    return 0;
}
