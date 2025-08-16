// Sum types example (Phase 4): construct, test, get, and match.
#include <iostream>
#include <string>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include <llvm/IR/Verifier.h>

using namespace edn;

int main(){
    const char* src = R"EDN(
        (module :name ex_sums
          (sum :name PairOrByte :variants [ (variant :name P :fields [ i32 i64 ]) (variant :name B :fields [ i8 ]) ])
          (fn :name "main" :ret i64 :params [ (param i32 %a) (param i64 %b) (param i8 %c) ] :body [
            (sum-new %s PairOrByte P [ %a %b ])
            (sum-is %isP PairOrByte %s P)
            (match %rv i64 PairOrByte %s
              :cases [
                (case P :binds [ (bind %x 0) (bind %y 1) ] :body [ ] :value %y)
                (case B :body [ ] :value %b)
              ]
              :default (default :body [ (const %z i32 0) ] :value %b))
            (ret i64 %rv)
          ])
        )
    )EDN";

    auto ast = parse(src);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tc;
    auto *M = emitter.emit(ast, tc);
    if(!tc.success || !M){
        std::cerr << "Type check failed (" << tc.errors.size() << ")\n";
        for(const auto& e : tc.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    std::string err; llvm::raw_string_ostream rso(err);
    if(llvm::verifyModule(*M, &rso)){
        std::cerr << rso.str(); return 2;
    }
    std::cout << "sum example OK\n";
    return 0;
}
