#include <cassert>
#include <iostream>
#include <string>

#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir_emitter.hpp"

#include <llvm/Support/raw_ostream.h>

using namespace edn;

static std::string module_to_ir(llvm::Module* m){ std::string buf; llvm::raw_string_ostream os(buf); m->print(os, nullptr); os.flush(); return buf; }

void run_phase4_generics_macro_test(){
    std::cout << "[phase4] generics via reader macros: basic gfn/gcall...\n";
    TypeContext tctx; IREmitter em(tctx); TypeCheckResult r;
    auto ast = parse(
        "(module :id \"gmod\""
        " (gfn :name \"id\" :generics [ T ] :ret T :params [ (param T %x) ] :body ["
        "   (ret T %x)"
        " ])"
        " (fn :name \"main\" :ret i32 :params [ (param i32 %a) ] :body ["
        "   (gcall %r i32 id :types [ i32 ] %a)"
        "   (ret i32 %r)"
        " ])"
        ")");
    auto *m = em.emit(ast, r); assert(r.success && m);
    // Verify that a specialized function exists with a mangled name and that main calls it
    std::string ir = module_to_ir(m);
    // Expect a definition of id@i32 and that main calls it (LLVM may quote names containing '@')
    if(ir.find("define i32 @\"id@i32\"(") == std::string::npos){
        std::cerr << "Expected specialization missing. IR:\n" << ir << std::endl;
    }
    assert(ir.find("define i32 @\"id@i32\"(") != std::string::npos);
    if(ir.find("call i32 @\"id@i32\"(") == std::string::npos){
        std::cerr << "Expected call to specialization missing. IR:\n" << ir << std::endl;
    }
    assert(ir.find("call i32 @\"id@i32\"(") != std::string::npos);
}
