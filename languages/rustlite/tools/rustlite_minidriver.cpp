#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/traits.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-min] building demo...\n";

    Builder b;
    b.begin_module()
     .rstruct("Vec2", { {"x","i32"}, {"y","i32"} })
      .fn_raw("field_demo", "i32", {},
          // Init Vec2 ptr, set x=3 via rset, then rget x and return it
          "[ (alloca %p Vec2) (const %zero i32 0) (rset i32 Vec2 %p x %zero) (const %three i32 3) (rset i32 Vec2 %p x %three) (rget %vx Vec2 %p x) (ret i32 %vx) ]")
      .fn_raw("addr_demo", "i32", {},
          // raddr/rderef round-trip
          "[ (const %v i32 5) (raddr %pv (ptr i32) %v) (rderef %r i32 %pv) (ret i32 %r) ]")
      .fn_raw("index_demo", "i32", {},
          // Build array [10,20,30] and read via rindex-load
          "[ (alloca %arr (array :elem i32 :size 3)) (const %ten i32 10) (const %twenty i32 20) (const %thirty i32 30) (const %i0 i32 0) (rindex-store i32 %arr %i0 %ten) (const %i1 i32 1) (rindex-store i32 %arr %i1 %twenty) (const %i2 i32 2) (rindex-store i32 %arr %i2 %thirty) (rindex-load %v i32 %arr %i2) (ret i32 %v) ]")
     .end_module();

    auto prog = b.build();
    auto ast = parse(prog.edn_text);
    auto expanded = expand_traits(rustlite::expand_rustlite(ast));

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-min] type check failed\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }

    IREmitter em(tctx); TypeCheckResult ir;
    auto *mod = em.emit(expanded, ir);
    assert(mod && ir.success);
    std::cout << "[rustlite-min] ok\n";
    return 0;
}
