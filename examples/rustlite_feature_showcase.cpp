// Rustlite feature showcase: tuples, arrays, enums + ematch, rtry, rwhile-let, ranges (inclusive),
// closure capture inference (flag), bounds-checked indexing (flag), bitwise & remainder, compound assign.
// This example exercises macro expansion paths and performs a simple runtime check via LLVM verify.
#include <iostream>
#include "edn/edn.hpp"
#include "edn/ir_emitter.hpp"
 #include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/rustlite.hpp" // ensure include path available via languages/rustlite/include in top-level build
#include "rustlite/expand.hpp"
#include <llvm/IR/Verifier.h>

using namespace edn; using namespace rustlite;

int main(){
    setenv("RUSTLITE_INFER_CAPS","1",1);
    setenv("RUSTLITE_BOUNDS","1",1);
  Builder b; b.begin_module();

    // Tuple + tget
    b.fn_raw("tuple_demo","i32", { {"i32","%a"}, {"i32","%b"} },
      "[ (tuple %t [ %a %b ]) (tget %x i32 %t 0) (tget %y i32 %t 1) (add %sum i32 %x %y) (ret i32 %sum) ]");

    // Enum + ematch
    b.sum_enum("Color", { {"Red", {}}, {"Green", {}}, {"Blue", {}}, {"Rgb", {"i32"}} });
    b.fn_raw("enum_demo","i32", { {"i32","%v"} },
      "[ (Color::Rgb %c %v) (ematch %out i32 Color %c :cases [ (case Red :body [ ] :value (i32-lit 0)) (case Green :body [ ] :value (i32-lit 1)) (case Blue :body [ ] :value (i32-lit 2)) (case Rgb :binds [ (bind %p0 0) ] :body [ ] :value %p0 ) ]) (ret i32 %out) ]");

    // Result + Option + rtry
    b.sum_enum("ResultI32", { {"Ok", {"i32"}}, {"Err", {"i32"}} });
    b.sum_enum("OptionI32", { {"Some", {"i32"}}, {"None", {}} });
    b.fn_raw("rtry_demo","i32", { {"i32","%base"} },
      "[ (const %b i32 %base) (rok %r1 ResultI32 %b) (rtry %x ResultI32 %r1) (rsome %s OptionI32 %x) (rtry %y OptionI32 %s) (add %z i32 %x %y) (ret i32 %z) ]");

    // rwhile-let over OptionI32 decrementing
    b.fn_raw("whilelet_demo","i32", { {"i32","%n"} },
      "[ (rsome %cur OptionI32 %n) (const %acc i32 0) (rwhile-let Some %cur :bind %v :body [ (add %acc2 i32 %acc %v) (assign %acc %acc2) (add %next i32 %v (i32-lit -1)) (rsome %cur OptionI32 %next) ]) (ret i32 %acc) ]");

    // Range demos
    b.fn_raw("range_demo","i32", {},
      "[ (const %acc i32 0) (rfor-range %i i32 0 4 :inclusive true :body [ (add %acc2 i32 %acc %i) (assign %acc %acc2) ]) (rrange %r i32 0 3 :inclusive false) (rfor-range %j i32 %r :body [ (add %acc3 i32 %acc %j) (assign %acc %acc3) ]) (ret i32 %acc) ]");

    // Bounds / operators / compound assign
    b.fn_raw("index_ops_demo","i32", {},
      "[ (arr %arr i32 [ (i32-lit 1) (i32-lit 2) (i32-lit 3) (i32-lit 4) ]) (const %idx i32 2) (rindex-load %v i32 %arr %idx :len (i32-lit 4)) (add %v2 i32 %v (i32-lit 5)) (rassign-op %v2 add (i32-lit 1)) (and %m i32 %v2 (i32-lit 7)) (ret i32 %m) ]");

    // Closure with inferred capture (previous const)
    b.fn_raw("closure_demo","i32", { {"i32","%seed"} },
      "[ (const %c i32 %seed) (rclosure %cl add_one :body [ (add %r i32 %c (i32-lit 1)) (ret i32 %r) ]) (add %out i32 %c (i32-lit 1)) (ret i32 %out) ]");

    // Main aggregator
    b.fn_raw("main","i32", {},
      "[ (call %a i32 tuple_demo (i32-lit 3) (i32-lit 4)) (call %b i32 enum_demo (i32-lit 9)) (call %c i32 rtry_demo (i32-lit 5)) (call %d i32 whilelet_demo (i32-lit 3)) (call %e i32 range_demo) (call %f i32 index_ops_demo) (call %g i32 closure_demo (i32-lit 10)) (add %s1 i32 %a %b) (add %s2 i32 %s1 %c) (add %s3 i32 %s2 %d) (add %s4 i32 %s3 %e) (add %s5 i32 %s4 %f) (add %s6 i32 %s5 %g) (ret i32 %s6) ]");

    b.end_module(); auto prog = b.build(); auto ast = parse(prog.edn_text);
    auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto tcr = tc.check_module(expanded);
    if(!tcr.success){ for(const auto &e : tcr.errors){ std::cerr << e.code << ": " << e.message << "\n"; } return 1; }
    IREmitter em(tctx); TypeCheckResult ir; auto *M = em.emit(expanded, ir); if(!ir.success || !M){ std::cerr << "Emit failed\n"; return 2; }
    std::string err; llvm::raw_string_ostream rso(err); if(llvm::verifyModule(*M, &rso)){ std::cerr << rso.str(); return 3; }
  std::cout << "rustlite feature showcase OK\n"; return 0; }
