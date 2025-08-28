#include <iostream>
#include <cassert>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/traits.hpp" // for trait macro expander (example reuse)
#include "rustlite/expand.hpp"
// LLVM IR introspection for PHI validation
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>

int main(int argc, char** argv){
    using namespace rustlite;
    using namespace edn;

    std::cout << "[rustlite] building demo...\n";
    Builder b;
    b.begin_module();
    b.sum_enum("OptionI32", { {"None", {}}, {"Some", {"i32"}} });
    b.sum_enum("ResultI32", { {"Ok", {"i32"}}, {"Err", {"i32"}} });
    // Helper functions for calls/closures
    b.fn_raw("adder", "i32", { {"env","i32"}, {"x","i32"} }, "[ (add %r i32 %env %x) (ret i32 %r) ]");
    b.fn_raw("id64", "i64", { {"x","i64"} }, "[ (ret i64 %x) ]");
    // Demos
    b.fn_raw("use_option", "i32",
        { {"p", "(ptr (struct-ref OptionI32))"} },
        "[ (match %ret i32 OptionI32 %p :cases [ (case None :body [ (const %z i32 0) :value %z ]) (case Some :binds [ (bind %x 0) ] :body [ :value %x ]) ] ) (ret i32 %ret) ]");
    b.fn_raw("use_option_rif", "i32",
        { {"p", "(ptr (struct-ref OptionI32))"} },
        "[ (rif-let %ret i32 OptionI32 Some %p :bind %x :then [ :value %x ] :else [ (const %z i32 0) :value %z ]) (ret i32 %ret) ]");
    b.fn_raw("let_demo", "i32", {}, "[ (const %init i32 41) (rlet i32 %a %init :body [ ]) (ret i32 %a) ]");
    b.fn_raw("mut_demo", "i32", {}, "[ (const %init i32 1) (rmut i32 %b %init :body [ (const %one i32 1) (add %tmp i32 %b %one) (assign %b %tmp) ]) (ret i32 %b) ]");
    b.fn_raw("if_demo", "i32", {}, "[ (const %one i32 1) (const %zero i32 0) (lt %c i32 %zero %one) (rif %c :then [ (ret i32 %one) ] :else [ (ret i32 %zero) ]) (ret i32 %one) ]");
    b.fn_raw("while_demo", "i32", {}, "[ (alloca %p i32) (const %one i32 1) (store i32 %p %one) (const %t i1 1) (rwhile %t :body [ (break) ]) (load %v i32 %p) (ret i32 %v) ]");
    b.fn_raw("for_demo", "i32", {}, "[ (rfor :init [ (const %cond i1 0) ] :cond %cond :step [ ] :body [ (continue) (break) ]) (const %z i32 2) (ret i32 %z) ]");
    b.fn_raw("match_demo", "i32", {}, "[ (const %seven i32 7) (rsome %opt OptionI32 %seven) (rmatch %r i32 OptionI32 %opt :arms [ (arm Some :binds [ %x ] :body [ :value %x ]) ] :else [ (const %z i32 0) :value %z ]) (const %one i32 1) (rerr %res ResultI32 %one) (rmatch %r2 i32 ResultI32 %res :arms [ (arm Ok :binds [ %v ] :body [ :value %v ]) ] :else [ (const %z2 i32 0) :value %z2 ]) (ret i32 %r) ]");
    b.fn_raw("call_demo", "i32", {}, "[ (const %sz i64 16) (rcall %r i64 id64 %sz) (const %zero i32 0) (ret i32 %zero) ]");
    b.fn_raw("closure_demo", "i32", {}, "[ (const %ten i32 10) (const %five i32 5) (rclosure %c adder :captures [ %ten ]) (rcall-closure %res i32 %c %five) (ret i32 %res) ]");
    b.fn_raw("types_demo", "i32", {}, "[ (const %z i32 0) (rret i32 %z) ]");
    b.fn_raw("assign_demo", "i32", {}, "[ (const %init i32 2) (rmut i32 %x %init :body [ (const %one i32 1) (add %t i32 %x %one) (rassign %x %t) ]) (ret i32 %x) ]");
    b.fn_raw("sc_demo", "i1", {}, "[ (const %t i1 1) (const %f i1 0) (rand %a %t %f) (ror %o %f %t) (ret i1 %a) ]");
    b.fn_raw("assert_demo", "i32", {}, "[ (const %one i32 1) (const %zero i32 0) (gt %c i32 %one %zero) (rassert %c) (const %ans i32 42) (ret i32 %ans) ]");
    b.fn_raw("assert2_demo", "i32", {}, "[ (const %t i1 1) (const %f i1 0) (rassert-eq %t %t) (rassert-ne %t %f) (const %ans i32 7) (ret i32 %ans) ]");
    b.end_module();

    auto prog = b.build();
    auto ast = parse(prog.edn_text);

    // Expand Rustlite surface first, then trait machinery
    auto expanded = expand_traits(rustlite::expand_rustlite(ast));

    TypeContext tctx;
    TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite] type check failed\n";
        std::cerr << "\n=== Generated EDN (pre-expand) ===\n" << prog.edn_text << "\n";
        // Scan for residual macro heads and unknown ops
        std::unordered_set<std::string> allow = {"module","sum","variant","fn","param","add","sub","mul","div","eq","ne","lt","le","gt","ge","const","ret","assign","as","alloca","store","load","while","for","block","if","match","case","bind","panic","assert","call","call-closure","make-closure","sum-new","trait-call","make-trait-obj","struct","field","typedef","extern-fn","global","rmatch","default"};
        std::unordered_set<std::string> seenUnknown;
        std::function<void(const edn::node_ptr&)> scan = [&](const edn::node_ptr& n){
            if(!n) return; if(std::holds_alternative<edn::vector_t>(n->data)){ for(auto &e: std::get<edn::vector_t>(n->data).elems) scan(e); return; }
            if(!std::holds_alternative<edn::list>(n->data)) return; auto &L = std::get<edn::list>(n->data).elems; if(L.empty()) return; if(std::holds_alternative<edn::symbol>(L[0]->data)){
                auto head = std::get<edn::symbol>(L[0]->data).name;
                if(!allow.count(head)) seenUnknown.insert(head);
            }
            for(auto &e: L) scan(e);
        }; scan(expanded);
        if(!seenUnknown.empty()){
            std::cerr << "[smoke-debug] unknown heads after macro expansion:";
            for(auto &h: seenUnknown) std::cerr << ' ' << h;
            std::cerr << "\n";
        }
        for(const auto& e : tcres.errors){
            std::cerr << e.code << ": " << e.message << "\n";
            for(const auto& n : e.notes){ std::cerr << "  note: " << n.message << "\n"; }
        }
        for(const auto& w : tcres.warnings){ std::cerr << "warn " << w.code << ": " << w.message << "\n"; }
        return 1;
    }

    IREmitter em(tctx);
    TypeCheckResult irres;
    auto *mod = em.emit(expanded, irres);
    assert(mod && irres.success);

    // Validate: no PHI nodes should have undef incoming values (guards result-mode match lowering)
    bool badPhi = false;
    for(auto &F : *mod){
        for(auto &BB : F){
            for(auto &I : BB){
                if(auto *phi = llvm::dyn_cast<llvm::PHINode>(&I)){
                    for(unsigned i=0;i<phi->getNumIncomingValues();++i){
                        llvm::Value* v = phi->getIncomingValue(i);
                        if(llvm::isa<llvm::UndefValue>(v)){
                            std::cerr << "[rustlite] ERROR: PHI has undef incoming in function '" << std::string(F.getName())
                                      << "' at block '" << std::string(phi->getIncomingBlock(i)->getName()) << "'\n";
                            badPhi = true;
                        }
                    }
                }
            }
        }
    }
    if(badPhi){
        return 2;
    }

    bool dump=false; for(int i=1;i<argc;++i){ if(std::string(argv[i])=="--dump") dump=true; }
    if(dump){
        std::cout << "\n=== High-level EDN ===\n" << prog.edn_text << "\n";
        std::cout << "\n=== Lowered LLVM IR ===\n";
        mod->print(llvm::outs(), nullptr);
        std::cout << "\n";
    }
    std::cout << "[rustlite] demo ok\n";
    return 0;
}
