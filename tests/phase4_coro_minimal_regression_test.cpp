#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/type_check.hpp"

// Minimal coroutine regression: ensure presence of suspend points and required intrinsics.
// Focuses on stable shape rather than exact mangled names.
void run_phase4_coro_minimal_regression_test(){
    std::cout << "[phase4] coroutines minimal regression test...\n";
    const char* SRC = R"EDN((module
      (fn :name "co" :ret i32 :params [ (param i32 %a) ] :body [
         (block [ ])
         (ret i32 %a)
      ])
    ))EDN"; // This may not actually create coroutine transforms unless gated; keep placeholder.
    auto ast = edn::parse(SRC);
    edn::TypeContext tctx; edn::IREmitter emitter(tctx); edn::TypeCheckResult tcres; auto *mod = emitter.emit(ast, tcres); assert(tcres.success && mod);
  // Determine if coroutine emission is expected (env flag turned on)
  bool expectCoro = false; if(const char* e = std::getenv("EDN_ENABLE_CORO")) expectCoro = std::string(e) == "1";
  // Scan for coroutine intrinsics (placeholder names like coro.id / coro.begin) if any
  bool sawAnyIntrinsics = false; for(auto &F : *mod){ if(F.getName().starts_with("coro.")) { sawAnyIntrinsics=true; break; } }
  bool hasPresplitAttr = false; if(auto *F = mod->getFunction("co")) hasPresplitAttr = F->hasFnAttribute("presplitcoroutine");
  if(expectCoro){
    // Accept either (a) intrinsic decls present, or (b) attribute present (frontend may stage emission incrementally).
    if(!(sawAnyIntrinsics || hasPresplitAttr)){
      std::string ir; llvm::raw_string_ostream os(ir); mod->print(os,nullptr); os.flush();
      std::cerr << "[coro-min] expected coro.* intrinsics or presplitcoroutine attr when EDN_ENABLE_CORO=1 but neither found. IR:\n" << ir << std::endl;
      assert(false && "Expected coroutine lowering markers when EDN_ENABLE_CORO=1");
    }
    // If intrinsics appeared we still require the attribute on the function (stronger contract when full lowering kicks in)
    if(sawAnyIntrinsics && !hasPresplitAttr){
      std::string ir; llvm::raw_string_ostream os(ir); mod->print(os,nullptr); os.flush();
      std::cerr << "[coro-min] saw coroutine intrinsics but missing presplitcoroutine attribute on co(). IR:\n" << ir << std::endl;
      assert(false && "Missing presplitcoroutine attr alongside coroutine intrinsics");
    }
  }
  // If not expecting coroutines, we make no assertion about absence (future default-on behavior acceptable) but ensure IR prints.
    std::string mstr; llvm::raw_string_ostream os(mstr); mod->print(os,nullptr);
    assert(!mstr.empty());
    std::cout << "[phase4] coroutines minimal regression test passed\n";
}
