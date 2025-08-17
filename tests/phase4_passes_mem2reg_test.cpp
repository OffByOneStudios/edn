// Verifies that with EDN_ENABLE_PASSES=1, simple alloca/store/load patterns
// are promoted to SSA (mem2reg), eliminating loads/stores in the function body.
#include <cassert>
#include <iostream>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"

void run_phase4_passes_mem2reg_test(){
    using namespace edn;
    std::cout << "[phase4] passes: mem2reg test...\n";

    const std::string src = R"EDN(
        (module
          (fn :name "promote" :ret i32 :params []
              :body [
                (alloca %px i32)
                (const %c i32 7)
                (store i32 %px %c)
                (load %x i32 %px)
                (ret i32 %x)
              ]))
    )EDN";

    auto count_mem_ops = [&](const char* flag)->std::pair<int,int>{
#if defined(_WIN32)
        _putenv_s("EDN_ENABLE_PASSES", flag);
#else
        setenv("EDN_ENABLE_PASSES", flag, 1);
#endif
        auto ast = parse(src);
        TypeContext tctx; TypeCheckResult tcres; IREmitter em(tctx);
        auto *m = em.emit(ast, tcres);
        assert(tcres.success && m);
        auto F = m->getFunction("promote");
        assert(F && "function promote should exist");
        int loads=0, stores=0;
        for(auto &bb : *F){
            for(auto &ins : bb){
                auto op = std::string(ins.getOpcodeName());
                if(op=="load") ++loads; else if(op=="store") ++stores;
            }
        }
        return {loads, stores};
    };

    auto before = count_mem_ops("0");
    assert(before.first >= 1 && before.second >= 1 && "expected loads/stores before mem2reg");

    auto after = count_mem_ops("1");
    assert(after.first == 0 && after.second == 0 && "mem2reg should remove loads/stores");

#if defined(_WIN32)
    _putenv_s("EDN_ENABLE_PASSES", "0");
#else
    setenv("EDN_ENABLE_PASSES", "0", 1);
#endif

    std::cout << "[phase4] passes: mem2reg test passed\n";
}
