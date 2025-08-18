#include <iostream>
#include <cassert>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/traits.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

static std::string module_to_ir(llvm::Module* m){
    std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf;
}

static void expect_contains(const std::string& ir, const char* needle){
    if(ir.find(needle)==std::string::npos){
        std::cerr << "[rustlite-traits] IR missing snippet:\n" << needle << "\n----- IR dump -----\n" << ir << "\n-------------------\n";
    }
    assert(ir.find(needle)!=std::string::npos);
}

int main(){
    std::cout << "[rustlite-traits] building demo...\n";
    // Minimal Rustlite trait-object call demo using Rustlite macros
    const char* edn =
        "(module :id \"rl_traits\" "
        "  (rtrait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ]) "
        "  (rfn :name \"print_i32\" :ret i32 :params [ (param (ptr i8) %ctx) (param i32 %v) ] :body [ (ret i32 %v) ]) "
        "  (fn :name \"trait_demo\" :ret i32 :params [ (param i32 %x) ] :body [ "
        "    (rfnptr %fp (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) print_i32) "
        "    (alloca %vt ShowVT) "
        "    (member-addr %p ShowVT %vt print) "
        "    (store (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) %p %fp) "
        "    (alloca %obj ShowObj) "
        "    (bitcast %vtp (ptr ShowVT) %vt) "
        "    (rmake-trait-obj %o Show %obj %vtp) "
        "    (rtrait-call %rv i32 Show %o print %x) "
        "    (ret i32 %rv) "
        "  ]) "
        ")";

    auto ast = parse(edn);
    // Order: expand Rustlite surface first (turn rtrait/rtrait-call into core forms), then expand traits
    auto expanded = expand_traits(rustlite::expand_rustlite(ast));

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-traits] type check failed\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }

    IREmitter em(tctx); TypeCheckResult ir;
    auto *mod = em.emit(expanded, ir);
    assert(mod && ir.success);
    auto irs = module_to_ir(mod);

    // Golden shape checks: vtable/object types and an indirect call present
    expect_contains(irs, "%struct.ShowVT = type { ptr }");
    expect_contains(irs, "%struct.ShowObj = type { ptr, ptr }");
    // Indirect call (function pointer) should appear as a call to a %tmp (not @symbol)
    expect_contains(irs, "call i32 %");
    // Access to vtable field should produce a GEP that mentions ShowVT
    expect_contains(irs, "getelementptr inbounds %struct.ShowVT");

    std::cout << "[rustlite-traits] ok\n";
    return 0;
}
