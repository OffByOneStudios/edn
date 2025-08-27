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
    std::cout << "[rustlite-index-addr] running...\n";
    // Exercise rindex-addr directly (address form) distinct from store/load helpers.
    // Pattern:
    //   (rarray %arr i32 4)
    //   (const %i2 i32 2) (const %val i32 77)
    //   (rindex-addr %p i32 %arr %i2)
    //   (store i32 %p %val)
    //   (rindex-load %got i32 %arr %i2)
    //   (rassert (eq i32 %got %val) "index-addr-store-load-mismatch")
    const char* edn_text =
        "(module :id \"rl_index_addr\" "
        "  (rfn :name \"idx_addr\" :ret i32 :params [ ] :body [ "
        "     (rarray %arr i32 4) "
        "     (const %i2 i32 2) (const %val i32 77) "
        "     (rindex-addr %p i32 %arr %i2) "
        "     (store i32 %p %val) "
        "     (rindex-load %got i32 %arr %i2) "
        "     (rassert (eq i32 %got %val) \"index-addr mismatch\") "
        "     (ret i32 %got) "
        "  ]) "
        ")";
    auto ast = parse(edn_text);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){ std::cerr << "[rustlite-index-addr] type check failed\n"; for(auto &e: tcres.errors) std::cerr<<e.code<<": "<<e.message<<"\n"; return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    auto irs = module_to_ir(mod);
    // Sanity: ensure a GEP pattern showing index 2 exists (rough heuristic)
    if(irs.find("getelementptr") == std::string::npos){ std::cerr << "[rustlite-index-addr] expected GEP not found in IR\n"; return 2; }
    std::cout << "[rustlite-index-addr] ok\n";
    return 0;
}
