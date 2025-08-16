#include <cassert>
#include <iostream>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/Support/raw_ostream.h>

using namespace edn;

static std::string module_to_ir(llvm::Module* m){ std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf; }

void run_phase4_generics_two_params_test(){
    std::cout << "[phase4] generics: two type params and call...\n";
    TypeContext tctx; IREmitter em(tctx); TypeCheckResult r;
    auto ast = parse(
        "(module :id \"g2\""
        " (gfn :name \"first\" :generics [ A B ] :ret A :params [ (param A %x) (param B %y) ] :body ["
        "   (ret A %x)"
        " ])"
        " (fn :name \"main\" :ret i32 :params [ (param i32 %a) (param i64 %b) ] :body ["
        "   (gcall %r i32 first :types [ i32 i64 ] %a %b)"
        "   (ret i32 %r)"
        " ])"
        ")");
    auto *m = em.emit(ast, r); assert(r.success && m);
    std::string ir = module_to_ir(m);
    // Expect a specialized function name with two type args
    assert(ir.find("define i32 @\"first@i32$i64\"(") != std::string::npos);
    assert(ir.find("call i32 @\"first@i32$i64\"(") != std::string::npos);
}

void run_phase4_generics_dedup_test(){
    std::cout << "[phase4] generics: deduplicate repeated instantiations...\n";
    TypeContext tctx; IREmitter em(tctx); TypeCheckResult r;
    auto ast = parse(
        "(module :id \"gdedup\""
        " (gfn :name \"id2\" :generics [ T ] :ret T :params [ (param T %x) ] :body ["
        "   (ret T %x)"
        " ])"
        " (fn :name \"f\" :ret i32 :params [ (param i32 %a) ] :body ["
        "   (gcall %r1 i32 id2 :types [ i32 ] %a)"
        "   (gcall %r2 i32 id2 :types [ i32 ] %r1)"
        "   (ret i32 %r2)"
        " ])"
        ")");
    auto *m = em.emit(ast, r); assert(r.success && m);
    std::string ir = module_to_ir(m);
    // There should be exactly one define for id2@i32
    size_t pos = 0, count = 0; std::string needle = "define i32 @\"id2@i32\"(";
    while((pos = ir.find(needle, pos)) != std::string::npos){ ++count; pos += needle.size(); }
    assert(count == 1);
    // And at least one call to it
    assert(ir.find("call i32 @\"id2@i32\"(") != std::string::npos);
}

static void neg(const char* program){
    TypeContext tctx; IREmitter em(tctx); TypeCheckResult r; auto ast = parse(program); auto *m = em.emit(ast, r); (void)m; assert(!r.success);
}

void run_phase4_generics_negative_tests(){
    std::cout << "[phase4] generics: negative shapes...\n";
    // Missing :types section -> expander will not rewrite; type checker should reject unknown 'gcall'
    neg("(module (gfn :name \"id\" :generics [ T ] :ret T :params [ (param T %x) ] :body [ (ret T %x) ]) (fn :name \"m\" :ret i32 :params [ (param i32 %a) ] :body [ (gcall %r i32 id %a) (ret i32 %r) ]))");
}
