// Surface test placeholder: until parser emits destructuring, we mimic its expected lowering.
#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"
using namespace edn; using namespace rustlite;
int main(){
    // Direct module text (matches style of other drivers). No deprecated Builder API.
    const char* src = R"((module :id "tuple_destructure_surface"
        (rfn :name "main" :ret i32 :params [] :body [
            (const %o i32 1) (const %p i32 2) (const %q i32 3)
            (tuple %t [ %o %p %q ])
            ; expected parser output for: let (a,b,c) = %t;
            (tget %a i32 %t 0) (tget %b i32 %t 1) (tget %c i32 %t 2)
            (add %ab i32 %a %b) (add %res i32 %ab %c) (ret i32 %res)
        ])
    ))";
    auto ast = parse(src);
    auto expanded = expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    if(!tcres.success){ std::cerr << "[tuple_destructure_surface] typecheck failed\n"; for(auto &e: tcres.errors) std::cerr<<e.code<<" "<<e.message<<"\n"; return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); (void)mod; if(!ir.success){ std::cerr<<"ir emit failed\n"; return 1; }
    std::cout << "tuple_destructure_surface OK\n"; return 0; }