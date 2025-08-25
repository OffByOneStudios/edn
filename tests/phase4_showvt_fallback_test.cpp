// Phase 4: ShowVT fallback emission gating test
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>
#include <llvm/Support/raw_ostream.h>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
#include "edn/traits.hpp"

using namespace edn;

static std::string to_ir(llvm::Module *m){ std::string s; llvm::raw_string_ostream os(s); m->print(os,nullptr); os.flush(); return s; }

// Build a minimal module that defines a trait (so normal vtable generation can occur) but does not
// rely on the forced fallback unless the env var is set.
static node_ptr build_module(){
  return parse(R"EDN((module :name svt
    (trait :name Show :methods [ (method :name print :type (ptr (fn-type :params [ (ptr i8) i32 ] :ret i32))) ])
    (fn :name "print_i32" :ret i32 :params [ (param (ptr i8) %ctx) (param i32 %v) ] :body [ (ret i32 %v) ])
  ))EDN");
}

void run_phase4_showvt_fallback_test(){
  std::cout << "[phase4] ShowVT fallback gating test...\n";
  // Case 1: Env var off -> fallback should NOT emit internal forcing global; ShowVT type may be absent if unused.
  #if defined(_WIN32)
    _putenv("EDN_FORCE_SHOWVT_FALLBACK="); // clear
  #else
    unsetenv("EDN_FORCE_SHOWVT_FALLBACK");
  #endif
  {
    auto ast = build_module();
    auto expanded = expand_traits(ast); // should synthesize ShowVT (natural) but fallback global gating concerns only forced emission path
    TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(expanded, r); assert(r.success && m);
    auto ir = to_ir(m);
    bool hasType = ir.find("%struct.ShowVT = type { ptr }") != std::string::npos;
    bool hasFallbackGlobal = ir.find("__edn.showvt.fallback") != std::string::npos;
    // Without flag we specifically expect NO fallback global.
    assert(!hasFallbackGlobal && "Fallback global emitted despite env flag being unset");
    // Type absence acceptable if trait codegen pruned it; if present, only one definition.
    if(hasType){
      size_t first = ir.find("%struct.ShowVT = type { ptr }");
      size_t second = ir.find("%struct.ShowVT = type { ptr }", first+1);
      assert(second == std::string::npos && "Duplicate ShowVT type definitions without fallback");
    }
  }
  // Case 2: Force fallback -> expect presence of ShowVT type (created if missing) AND internal forcing global symbol.
  #if defined(_WIN32)
    _putenv_s("EDN_FORCE_SHOWVT_FALLBACK", "1");
  #else
    setenv("EDN_FORCE_SHOWVT_FALLBACK", "1", 1);
  #endif
  {
    auto ast = build_module();
    auto expanded = expand_traits(ast);
    TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto *m = em.emit(expanded, r); assert(r.success && m);
    auto ir = to_ir(m);
    size_t first = ir.find("%struct.ShowVT = type { ptr }"); assert(first != std::string::npos && "Fallback should create ShowVT struct type");
    size_t second = ir.find("%struct.ShowVT = type { ptr }", first+1); assert(second == std::string::npos && "Duplicate ShowVT type definitions with fallback");
    bool hasFallbackGlobal = ir.find("__edn.showvt.fallback") != std::string::npos;
    if(!hasFallbackGlobal){
      std::cerr << "[showvt] expected __edn.showvt.fallback global under fallback flag. IR snippet:\n";
      std::cerr << ir.substr(0, std::min<size_t>(500, ir.size())) << std::endl;
      assert(false && "Missing fallback global");
    }
  }
  std::cout << "[phase4] ShowVT fallback gating test passed\n";
}
