#include <iostream>
#include <cassert>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

static std::string module_to_ir(llvm::Module* m){
    std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf;
}

static void expect_contains(const std::string& ir, const char* needle){
    if(ir.find(needle)==std::string::npos){
        std::cerr << "[rustlite-extern] IR missing snippet:\n" << needle << "\n----- IR dump -----\n" << ir << "\n-------------------\n";
    }
    assert(ir.find(needle)!=std::string::npos);
}

int main(){
    std::cout << "[rustlite-extern] building demo...\n";
    // Extern C call smoke: declare abs(int) and call it
    const char* edn =
        "(module :id \"rl_extern\" "
        "  (rextern-fn :name \"abs\" :ret i32 :params [ (param i32 %x) ]) "
        "  (rfn :name \"extern_demo\" :ret i32 :params [ ] :body [ "
        "    (const %m i32 -5) "
        "    (rcall %r i32 abs %m) "
        "    (ret i32 %r) "
        "  ]) "
        ")";

    auto ast = parse(edn);
    auto expanded = rustlite::expand_rustlite(ast);

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-extern] type check failed\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }

    IREmitter em(tctx); TypeCheckResult ir;
    auto *mod = em.emit(expanded, ir);
    assert(mod && ir.success);
    auto irs = module_to_ir(mod);

    // Expect an external declaration and a direct call
    // Note: LLVM may include qualifiers like dso_local; match key parts only
    expect_contains(irs, "declare i32 @abs(i32"
    );
    expect_contains(irs, "call i32 @abs(");

    std::cout << "[rustlite-extern] ok\n";
    return 0;
}
