#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

// Test: generic function monomorphization produces distinct specialized function symbols
// and rcall-g sites are rewritten to plain call of those specialized symbols.
// Current naming scheme: <base>__<TyArgs joined by _>

using namespace edn;

static std::string dump_functions(llvm::Module *m){
    std::string out; out.reserve(256);
    for(auto &F : *m){ if(F.isDeclaration()) continue; out += F.getName().str(); out += '\n'; }
    return out;
}

int main(){
    const char* SRC = R"EDN((module
        (fn :name "id" :generics [ T ] :ret T :params [ (param T %x) ] :body [ (ret T %x) ])
        (fn :name "use" :ret i32 :params [ ] :body [
            (const %a i32 3)
            (rcall-g %r1 i32 id [ i32 ] %a)
            (const %b f64 4)
            (rcall-g %r2 f64 id [ f64 ] %b)
            (ret i32 %r1)
        ])
    ))EDN";
    auto ast = parse(SRC);
    auto expanded = rustlite::expand_rustlite(ast);
    // Validate that source no longer contains rcall-g and that specializations are present.
    bool saw_rcall_g=false; bool saw_i32=false; bool saw_f64=false;
    if(expanded && std::holds_alternative<list>(expanded->data)){
        auto &ML = std::get<list>(expanded->data).elems;
        for(auto &n : ML){ if(!n||!std::holds_alternative<list>(n->data)) continue; auto &L=std::get<list>(n->data).elems; if(L.empty()) continue; if(std::holds_alternative<symbol>(L[0]->data)){
            std::string head = std::get<symbol>(L[0]->data).name;
            if(head=="fn"){
                // scan name
                for(size_t i=1;i+1<L.size(); i+=2){ if(!std::holds_alternative<keyword>(L[i]->data)) break; auto kw=std::get<keyword>(L[i]->data).name; if(kw=="name" && std::holds_alternative<std::string>(L[i+1]->data)){
                    auto fname = std::get<std::string>(L[i+1]->data);
                    if(fname=="id__i32") saw_i32=true; if(fname=="id__f64") saw_f64=true;
                } }
            }
            if(head=="rcall-g") saw_rcall_g=true;
        }}
    }
    if(!saw_i32 || !saw_f64 || saw_rcall_g){ std::cerr<<"specialization expansion invalid"<<"\n"; return 1; }
    TypeContext tctx; TypeChecker tc(tctx); auto tcres = tc.check_module(expanded); if(!tcres.success){ std::cerr<<"type check failed"<<"\n"; return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *m = em.emit(expanded, ir); if(!m || !ir.success){ std::cerr<<"ir emit failed"<<"\n"; return 1; }
    auto fnlist = dump_functions(m);
    if(fnlist.find("id__i32")==std::string::npos || fnlist.find("id__f64")==std::string::npos){ std::cerr<<"expected specialized functions in IR"<<"\n"; return 1; }
    std::cout<<"[rustlite-generics-specialization-ir] ok"<<"\n"; return 0;
}
