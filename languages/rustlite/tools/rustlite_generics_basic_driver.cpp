// Basic generics prototype: single-type parameter identity function monomorphized twice
#include <iostream>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

int main(){
    // Prototype uses two manually duplicated rfn forms simulating future generic expansion
    const char* mod = "(module (rfn :name \"id_i32\" :ret i32 :params [ (param i32 %x) ] :body [ (ret i32 %x) ]) (rfn :name \"id_f64\" :ret f64 :params [ (param f64 %x) ] :body [ (ret f64 %x) ]) (rfn :name \"use\" :ret i32 :params [ ] :body [ (const %a i32 9) (rcall %r1 i32 id_i32 %a) (const %b f64 2) (rcall %r2 f64 id_f64 %b) (ret i32 %r1) ]))";
    auto ast=parse(mod); auto expanded=rustlite::expand_rustlite(ast); TypeContext tctx; TypeChecker tc(tctx); auto tcres=tc.check_module(expanded); if(!tcres.success){ for(auto &e: tcres.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *llvm_mod=em.emit(expanded, ir); if(!llvm_mod||!ir.success){ std::cerr<<"emit failed\n"; return 1; }
    std::cout<<"[rustlite-generics-basic] ok\n"; return 0; }
