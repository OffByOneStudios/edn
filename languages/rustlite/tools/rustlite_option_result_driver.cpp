#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

// Dedicated driver to exercise Option / Result construction and matching macros
// Covers: rnone, rsome, rok, rerr, rmatch (both Option and Result) and rif-let sugar for Option

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    b.sum_enum("OptionI32", { {"None", {}}, {"Some", {"i32"}} });
    b.sum_enum("ResultI32", { {"Ok", {"i32"}}, {"Err", {"i32"}} });

    // Function that builds Some(7) and matches extracting 7
    b.fn_raw("opt_some_match", "i32", {},
        "[ (const %seven i32 7) (rsome %o OptionI32 %seven) (rmatch %r i32 OptionI32 %o :arms [ (arm Some :binds [ %x ] :body [ :value %x ]) ] :else [ (const %z i32 0) :value %z ]) (ret i32 %r) ]");
    // Function that builds None and matches -> 0
    b.fn_raw("opt_none_match", "i32", {},
        "[ (rnone %o OptionI32) (rmatch %r i32 OptionI32 %o :arms [ (arm Some :binds [ %x ] :body [ :value %x ]) ] :else [ (const %z i32 0) :value %z ]) (ret i32 %r) ]");
    // Function that uses rif-let to extract Some path or fallback
    b.fn_raw("opt_rif_let", "i32", {},
        "[ (const %ten i32 10) (rsome %o OptionI32 %ten) (rif-let %r i32 OptionI32 Some %o :bind %x :then [ :value %x ] :else [ (const %z i32 0) :value %z ]) (ret i32 %r) ]");

    // Result: Ok(5) matched
    b.fn_raw("res_ok_match", "i32", {},
        "[ (const %five i32 5) (rok %rval ResultI32 %five) (rmatch %r i32 ResultI32 %rval :arms [ (arm Ok :binds [ %v ] :body [ :value %v ]) ] :else [ (const %z i32 0) :value %z ]) (ret i32 %r) ]");
    // Result: Err(2) matched -> 0 branch
    b.fn_raw("res_err_match", "i32", {},
        "[ (const %two i32 2) (rerr %rval ResultI32 %two) (rmatch %r i32 ResultI32 %rval :arms [ (arm Ok :binds [ %v ] :body [ :value %v ]) ] :else [ (const %z i32 0) :value %z ]) (ret i32 %r) ]");

    b.end_module();
    auto prog = b.build();
    auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        for(const auto &e: tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult irres; auto *mod = em.emit(expanded, irres); assert(mod && irres.success);
    // (Optionally we could run simple sanity by scanning for PHI undef but not needed here.)
    std::cout << "[rustlite] option_result driver ok\n";
    return 0;
}
