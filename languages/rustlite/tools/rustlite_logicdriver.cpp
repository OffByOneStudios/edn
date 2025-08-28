#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite;
    using namespace edn;
    std::cout << "[rustlite-logic] building demo...\n";
    Builder b; b.begin_module();
    // Functions exercising rand/ror with simple constants and equality assertions
    b.fn_raw("logic", "i32", {},
        "[ "
        "  (const %t i1 1) (const %f i1 0) "
        "  (rand %a %t %f) (rassert-eq %a %f) "
        "  (rand %b %t %t) (rassert-eq %b %t) "
        "  (rand %c %f %t) (rassert-eq %c %f) "
        "  (ror %d %t %f)  (rassert-eq %d %t) "
        "  (ror %e %f %t)  (rassert-eq %e %t) "
        "  (ror %g %f %f)  (rassert-eq %g %f) "
        "  (const %ok i32 0) (ret i32 %ok) "
        "]");
    b.end_module();

    auto prog = b.build();
    auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);
    // DEBUG: scan for any residual macro heads beginning with 'r' that should have been expanded
    std::function<void(const edn::node_ptr&)> scan = [&](const edn::node_ptr& n){
        if(!n) return; if(std::holds_alternative<edn::vector_t>(n->data)){ for(auto &e: std::get<edn::vector_t>(n->data).elems) scan(e); return; }
        if(!std::holds_alternative<edn::list>(n->data)) return; auto &L = std::get<edn::list>(n->data).elems; if(L.empty()) return; if(std::holds_alternative<edn::symbol>(L[0]->data)){
            auto head = std::get<edn::symbol>(L[0]->data).name; if(head.size()>2 && head[0]=='r' && std::isalpha(static_cast<unsigned char>(head[1])) && head.rfind("ret",0)!=0){
                std::cerr << "[logic-debug] leftover head: " << head << "\n";
            }
        }
        for(auto &e: L) scan(e);
    }; scan(expanded);

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-logic] type check failed\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }

    IREmitter em(tctx); TypeCheckResult ir;
    auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    std::cout << "[rustlite-logic] ok\n";
    return 0;
}
