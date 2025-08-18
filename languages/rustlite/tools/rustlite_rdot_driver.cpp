#include <iostream>
#include <cassert>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/traits.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

static std::string module_to_ir(llvm::Module* m){ std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf; }
static void expect_contains(const std::string& ir, const char* needle){ assert(ir.find(needle)!=std::string::npos); }

int main(){
    std::cout << "[rustlite-rdot] building demo...\n";
    const char* edn =
        "(module :id \"rl_rdot\" "
        "  (rtrait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ]) "
        "  (rfn :name \"print_i32\" :ret i32 :params [ (param (ptr i8) %ctx) (param i32 %v) ] :body [ (ret i32 %v) ]) "
    "  (fn :name \"use_trait\" :ret i32 :params [ (param i32 %x) ] :body [ "
        "    (rfnptr %fp (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) print_i32) "
        "    (alloca %vt ShowVT) (member-addr %p ShowVT %vt print) (store (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32)) %p %fp) "
        "    (alloca %obj ShowObj) (bitcast %vtp (ptr ShowVT) %vt) (rmake-trait-obj %o Show %obj %vtp) "
    "    (rdot %rv i32 Show %o print %x) (ret i32 %rv) "
    "  ]) "
    "  (fn :name \"use_free\" :ret i32 :params [ (param i32 %x) ] :body [ (alloca %ctx i8) (rdot %rv i32 print_i32 %ctx %x) (ret i32 %rv) ]) "
        ")";

    auto ast = parse(edn);
    auto expanded = expand_traits(rustlite::expand_rustlite(ast));

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-rdot] type check failed\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    auto irs = module_to_ir(mod);
    // Expect at least one indirect call and one direct call
    expect_contains(irs, "call i32 %");
    expect_contains(irs, "call i32 @print_i32");

    std::cout << "[rustlite-rdot] ok\n";
    return 0;
}
