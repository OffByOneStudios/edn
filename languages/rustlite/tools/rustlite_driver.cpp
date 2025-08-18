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
    b.begin_module()
     .sum_enum("OptionI32", { {"None", {}}, {"Some", {"i32"}} })
     .fn_raw("use_option", "i32",
        { {"p", "(ptr (struct-ref OptionI32))"} },
        // This body uses the built-in `match` form provided by EDN core.
        "[ (match %ret i32 OptionI32 %p :cases [ (case None :body [ (const %z i32 0) :value %z ]) (case Some :binds [ (bind %x 0) ] :body [ :value %x ]) ] ) (ret i32 %ret) ]")
      .fn_raw("use_option_rif", "i32",
          { {"p", "(ptr (struct-ref OptionI32))"} },
          // rif-let result-mode: binds Some(x) to %x -> returns x, else 0
          "[ (rif-let %ret i32 OptionI32 Some %p :bind %x :then [ :value %x ] :else [ (const %z i32 0) :value %z ]) (ret i32 %ret) ]")
     .fn_raw("let_demo", "i32",
         { },
         // rlet: declare %a: i32, initialized from %init, then return it outside the block
         "[ (const %init i32 41) (rlet i32 %a %init :body [ ]) (ret i32 %a) ]")
     .fn_raw("mut_demo", "i32",
         { },
         // rmut: declare %b: i32, initialized from %init, then mutate and return outside the block
         "[ (const %init i32 1) (rmut i32 %b %init :body [ (const %one i32 1) (add %tmp i32 %b %one) (assign %b %tmp) ]) (ret i32 %b) ]")
     .end_module();

    auto prog = b.build();
    auto ast = parse(prog.edn_text);

    // Expand any shared macros (e.g., traits) if present; for now, sums/match are core, so expansion is identity.
    auto expanded = rustlite::expand_rustlite(expand_traits(ast));

    TypeContext tctx;
    TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite] type check failed\n";
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
