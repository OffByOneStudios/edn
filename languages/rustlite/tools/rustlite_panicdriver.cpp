#include <iostream>
#include <cassert>
#include <string>
#include <cstdlib>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

static std::string module_to_ir(llvm::Module* m){
    std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf;
}

static void expect_contains(const std::string& ir, const char* needle){
    if(ir.find(needle)==std::string::npos){
        std::cerr << "[rustlite-panic] IR missing snippet:\n" << needle << "\n----- IR dump -----\n" << ir << "\n-------------------\n";
    }
    assert(ir.find(needle)!=std::string::npos);
}

static void expect_not_contains(const std::string& ir, const char* needle){
    if(ir.find(needle)!=std::string::npos){
        std::cerr << "[rustlite-panic] IR unexpectedly contains snippet:\n" << needle << "\n----- IR dump -----\n" << ir << "\n-------------------\n";
    }
    assert(ir.find(needle)==std::string::npos);
}

int main(){
    std::cout << "[rustlite-panic] building demo...\n";
    const char* edn =
        "(module :id \"rl_panic\" "
        "  (rfn :name \"boom\" :ret Void :params [ ] :body [ (rpanic) ]) "
        ")";

    auto ast = parse(edn);
    auto expanded = rustlite::expand_rustlite(ast);

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-panic] type check failed\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }

    IREmitter em(tctx); TypeCheckResult ir;
    auto *mod = em.emit(expanded, ir);
    assert(mod && ir.success);
    auto irs = module_to_ir(mod);

    // Decide expectations from env
    const char* eh = std::getenv("EDN_ENABLE_EH");
    const char* pm = std::getenv("EDN_PANIC");
    bool wantUnwind = (eh && std::string(eh)=="1" && pm && std::string(pm)=="unwind");

    if(wantUnwind){
        // Expect a personality and a call/invoke to RaiseException (SEH) or __cxa_throw (Itanium)
        bool hasPersonality = (irs.find("__C_specific_handler")!=std::string::npos) || (irs.find("__gxx_personality_v0")!=std::string::npos);
        if(!hasPersonality){
            std::cerr << "[rustlite-panic] missing personality in IR\n" << irs << std::endl; assert(false);
        }
        bool hasUnwindCall = (irs.find("RaiseException")!=std::string::npos) || (irs.find("__cxa_throw")!=std::string::npos);
        if(!hasUnwindCall){
            std::cerr << "[rustlite-panic] missing unwind throw call in IR\n" << irs << std::endl; assert(false);
        }
    }else{
        // Abort mode: should lower to llvm.trap + unreachable; no invoke and no personalities
        expect_contains(irs, "@llvm.trap");
        expect_contains(irs, "unreachable");
        expect_not_contains(irs, "invoke ");
        expect_not_contains(irs, "__C_specific_handler");
        expect_not_contains(irs, "__gxx_personality_v0");
    }

    std::cout << "[rustlite-panic] ok\n";
    return 0;
}
