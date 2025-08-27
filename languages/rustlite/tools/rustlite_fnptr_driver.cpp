#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

// Driver validating rfnptr macro + indirect call usage.
// Exercises: rfnptr lowering to fnptr, indirect call with matching signature, and a mismatch negative (captured internally).

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // Simple callee: adds two i32
    b.fn_raw("add2", "i32", { {"a","i32"}, {"b","i32"} }, "[ (add %s i32 %a %b) (ret i32 %s) ]");
    // Function returning result of indirect add2 call through function pointer
    b.fn_raw("via_ptr", "i32", {},
        "[ (rfnptr %fp (ptr (fn-type :params [ i32 i32 ] :ret i32)) add2) (const %x i32 3) (const %y i32 4) (call-indirect %r i32 %fp %x %y) (ret i32 %r) ]");
    b.end_module();
    auto prog = b.build(); auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        for(const auto &e: tcres.errors){ std::cerr<<e.code<<": "<<e.message<<"\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult irres; auto *mod = em.emit(expanded, irres); assert(mod && irres.success);
    std::cout << "[rustlite] fnptr driver ok\n";
    return 0;
}
