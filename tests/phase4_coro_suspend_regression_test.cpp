#include <iostream>
#include <cassert>
#include <cstdlib>
#include <string>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"
#include <llvm/Support/raw_ostream.h>

// Comprehensive coroutine suspend/final-suspend regression:
// Ensures that with EDN_ENABLE_CORO=1 a representative sequence of coro-* ops
// emits expected intrinsic calls and that the function has presplitcoroutine.
void run_phase4_coro_suspend_regression_test(){
    std::cout << "[phase4] coroutines suspend/final-suspend regression test...\n";
#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_CORO", "1");
    _putenv_s("EDN_ENABLE_PASSES", "0");
#else
    setenv("EDN_ENABLE_CORO", "1", 1);
    setenv("EDN_ENABLE_PASSES", "0", 1);
#endif
        // NOTE: Type checker rules:
        //  - coro-begin yields a handle %h and implicitly sets lastCoroIdTok
        //  - coro-id materializes that token into a %var (required for coro-alloc)
        //  - coro-alloc takes the id token, NOT the handle
        //  - coro-save takes the handle and produces a token (not needed for suspend here but exercised)
        //  - coro-suspend / coro-final-suspend both take the HANDLE (not the save token)
        // Sequence below reflects those invariants.
            const char* SRC = R"EDN((module
                (fn :name "co_full" :ret void :params [ (param i32 %a) ] :body [ ; implicit void return (no explicit ret required)
                    (coro-begin %h)
                    (coro-id %cid)
                    (coro-save %tok %h)
                    (coro-promise %p %h)
                    (coro-size %sz)
                    (coro-alloc %need %cid)
                    (coro-suspend %s %h)
                    (coro-final-suspend %fs %tok)
                (coro-end %h)
                ])
            ))EDN";
    auto ast = edn::parse(SRC);
    edn::TypeContext tctx; edn::IREmitter emitter(tctx); edn::TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres);
    if(!(tcres.success && mod)){
        std::cerr << "[coro-suspend-reg] type check failed (success=" << tcres.success << ") errors=" << tcres.errors.size() << "\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << " (hint=" << e.hint << ")\n"; }
        assert(false && "coro suspend regression type check failed");
    }
    auto *F = mod->getFunction("co_full"); assert(F);
    bool hasPresplit = F->hasFnAttribute("presplitcoroutine");
    std::string ir; llvm::raw_string_ostream os(ir); mod->print(os,nullptr); os.flush();
    auto mustFind = [&](const char* needle){ if(ir.find(needle)==std::string::npos){ std::cerr << "[coro-suspend-reg] missing snippet: " << needle << "\n"; std::cerr << ir << std::endl; assert(false); } };
    mustFind("coro.id");
    mustFind("coro.begin");
    mustFind("coro.save");
    mustFind("coro.suspend"); // ensure at least one suspend emitted
    // Ensure final suspend variant (i1 true) also present asserting second form
    mustFind("i1 true");
    mustFind("coro.end");
    if(!hasPresplit){
        std::cerr << "[coro-suspend-reg] function missing presplitcoroutine attribute though coroutines enabled.\n" << ir << std::endl;
        assert(false);
    }
    std::cout << "[phase4] coroutines suspend/final-suspend regression test passed\n";
}
