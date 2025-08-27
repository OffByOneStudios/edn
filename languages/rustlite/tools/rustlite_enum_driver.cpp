#include <iostream>
#include <cassert>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

static std::string module_to_ir(llvm::Module* m){ std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf; }

int main(){
    std::cout << "[rustlite-enum] building renum + rmatch demo...\n";
    const char* edn =
        "(module :id \"rl_enum\" "
        "  (renum :name Opt :variants [ (Nil) (Val i32) ]) "
        "  (rfn :name \"use_sum\" :ret i32 :params [ (param i32 %x) ] :body [ "
        "    (const %zero i32 0) "
        "    (rsum %s Opt Val :vals [ %x ]) "
        "    (rmatch %out i32 Opt %s :arms [ (arm Val :binds [ %v ] :body [ (as %out i32 %v) ]) ] :else [ (as %out i32 %zero) ]) "
        "    (ret i32 %out) "
        "  ]) "
        ")";

    auto ast = parse(edn);
    auto expanded = rustlite::expand_rustlite(ast); // renum + rmatch + rfn expansion

    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(!tcres.success){
        std::cerr << "[rustlite-enum] type check failed\n";
        for(const auto& e : tcres.errors){ std::cerr << e.code << ": " << e.message << "\n"; }
        return 1;
    }
    IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
    auto irs = module_to_ir(mod);
    // Basic shape assertions
    assert(irs.find("Opt") != std::string::npos);
    assert(irs.find("use_sum") != std::string::npos);
    std::cout << "[rustlite-enum] ok\n";
    return 0;
}
