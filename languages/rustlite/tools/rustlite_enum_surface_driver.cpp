#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

int main(){
    std::cout << "[rustlite-enum-surface] enum surface macro demo...\n";
    const char* edn =
        "(module :id \"enum_surface\" "
        "  (enum :name Color :variants [ (Red) (Green) (Blue) ]) "
        "  (rfn :name \"pick\" :ret i32 :params [ (param i32 %idx) ] :body [ "
        "    (const %zero i32 0) (const %one i32 1) (const %two i32 2) "
    "    (enum-ctor %c Color Red) "
    "    (rmatch %out i32 Color %c :arms [ "
    "        (arm Red   :body [ :value %zero ]) "
    "        (arm Green :body [ :value %one ]) "
    "        (arm Blue  :body [ :value %two ]) "
    "      ] :else [ :value %zero ]) "
        "    (ret i32 %out) "
        "  ]) "
        ")";

    auto ast = parse(edn);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        for(const auto &e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    std::cout << "[rustlite-enum-surface] ok\n";
    return 0;
}
