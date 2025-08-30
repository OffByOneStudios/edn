// Generic parameter syntax driver: single generic function instantiated twice via rcall-g
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

int main(){
    const char* mod = R"EDN((module
        (fn :name "id" :generics [ T ] :ret T :params [ (param T %x) ] :body [ (ret T %x) ])
        (fn :name "use" :ret i32 :params [ ] :body [
            (const %a i32 7)
            (rcall-g %r1 i32 id [ i32 ] %a)
            (const %b f64 2)
            (rcall-g %r2 f64 id [ f64 ] %b)
            (ret i32 %r1)
        ])
    ))EDN";
    auto ast=parse(mod);
    auto expanded=rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres=tc.check_module(expanded);
    if(!tcres.success){ for(auto &e: tcres.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *llvm_mod=em.emit(expanded, ir); if(!llvm_mod||!ir.success){ std::cerr<<"emit failed\n"; return 1; }
    std::cout<<"[rustlite-generics-param] ok\n"; return 0; }
