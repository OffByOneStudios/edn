#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/traits.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-neg] building demo...\n";
    Builder b;
    b.begin_module()
     .rstruct("S", { {"x","i32"} })
     // Misuse 1: rset with non-pointer base
     .fn_raw("bad_rset_base", "i32", {},
        "[ (alloca %p S) (const %v i32 1) (rget %tmp S %p x) (rset i32 S %tmp x %v) (ret i32 %v) ]")
     // Misuse 2: rindex with non-integer index
     .fn_raw("bad_rindex_idx", "i32", {},
        "[ (alloca %arr (array :elem i32 :size 2)) (const %i i32 1) (const %f f32 1) (rindex %v i32 %arr %f) (ret i32 %i) ]")
     .end_module();

    auto ast = parse(b.build().edn_text);
    auto expanded = rustlite::expand_rustlite(expand_traits(ast));
    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(tcres.success){ std::cerr << "[rustlite-neg] expected type check failures but passed" << std::endl; return 1; }
    // Print the specific errors we expect exist (presence is sufficient for this smoke)
    bool saw_rset_ptr=false, saw_idx_int=false;
    for(const auto& e : tcres.errors){
        if(e.code=="E0815" || e.code=="E0213" || e.code=="E0212") saw_rset_ptr = true; // member-addr base must be pointer OR store ptr mismatch
        if(e.code=="E0825") saw_idx_int = true; // index must be int
    }
    if(!saw_rset_ptr || !saw_idx_int){
        std::cerr << "[rustlite-neg] missing expected errors: rset-ptr=" << (saw_rset_ptr?"ok":"miss")
                  << ", idx-int=" << (saw_idx_int?"ok":"miss") << std::endl; return 2;
    }
    std::cout << "[rustlite-neg] saw expected errors\n";
    return 0;
}
