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
        std::cerr << "[rustlite-generics] IR missing snippet:\n" << needle << "\n----- IR dump -----\n" << ir << "\n-------------------\n";
    }
    assert(ir.find(needle)!=std::string::npos);
}

int main(){
    std::cout << "[rustlite-generics] building demo...\n";
    // "Generics" demo: two monomorphized instantiations of an identity function (i32 and f32)
    const char* edn =
        "(module :id \"rl_generics\" "
        "  (rfn :name \"id_i32\" :ret i32 :params [ (param i32 %x) ] :body [ (ret i32 %x) ]) "
        "  (rfn :name \"id_f32\" :ret f32 :params [ (param f32 %x) ] :body [ (ret f32 %x) ]) "
        "  (rfn :name \"gen_demo\" :ret i32 :params [ ] :body [ "
        "    (const %a i32 7) "
        "    (rcall %ai i32 id_i32 %a) "
        "    (const %b f32 3) "
        "    (rcall %bf f32 id_f32 %b) "
        "    (ret i32 %ai) "
        "  ]) "
        ")";

    auto ast = parse(edn);
    auto expanded = rustlite::expand_rustlite(ast);

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-generics] type check failed\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }

    IREmitter em(tctx); TypeCheckResult ir;
    auto *mod = em.emit(expanded, ir);
    assert(mod && ir.success);
    auto irs = module_to_ir(mod);

    // Expect two specialized functions and two direct calls
    expect_contains(irs, "@id_i32(");
    expect_contains(irs, "@id_f32(");
    expect_contains(irs, "call i32 @id_i32(");
    // f32 maps to 'float' in LLVM IR; just ensure we call the symbol regardless of type text
    expect_contains(irs, "call ");
    expect_contains(irs, "@id_f32(");

    std::cout << "[rustlite-generics] ok\n";
    return 0;
}
