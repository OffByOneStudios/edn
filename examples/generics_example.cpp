// Generics example (Phase 4): instantiate a simple identity generic.
#include <iostream>
#include <string>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include <llvm/IR/Verifier.h>

using namespace edn;

int main(){
    const char* src = R"EDN(
        (module :name ex_generics
          (gfn :name "id" :generics [ T ] :ret T :params [ (param T %x) ] :body [ (ret T %x) ])
          (fn :name "main" :ret i32 :params [ (param i32 %a) ] :body [
            (gcall %r i32 id :types [ i32 ] %a)
            (ret i32 %r)
          ])
        )
    )EDN";

    auto ast = parse(src);
    TypeContext tctx; IREmitter emitter(tctx); TypeCheckResult tc;
    auto *M = emitter.emit(ast, tc);
    if(!tc.success || !M){
        std::cerr << "Type check failed (" << tc.errors.size() << ")\n";
        return 1;
    }
    std::string err; llvm::raw_string_ostream rso(err);
    if(llvm::verifyModule(*M, &rso)){
        std::cerr << rso.str(); return 2;
    }
    std::cout << "generics example OK\n";
    return 0;
}
