#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/ir_emitter.hpp"

using namespace edn;

void run_ir_emitter_test(){
    TypeContext ctx;
    // add function module
    {
    std::cout << "IR test: starting add module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_add\" (fn :name \"add\" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [ (add %s i32 %a %b) (ret i32 %s) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("add"); assert(fn); bool saw=false; for(auto &bb:*fn) for(auto &ins:bb) if(ins.getOpcodeName()==std::string("add")) saw=true; assert(saw);
    }
    // index (array + gep) function module
    {
    std::cout << "IR test: starting index module\n";
        IREmitter em(ctx);
        // allocate array [4 x i32], get pointer to element 2, store then load
    auto ast = parse("(module :id \"m_index\" (fn :name \"idx\" :ret i32 :params [] :body [ (alloca %p (array :elem i32 :size 4)) (const %c i32 7) (const %i i32 2) (index %e i32 %p %i) (store i32 %e %c) (load %v i32 %e) (ret i32 %v) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("idx"); assert(fn);
        bool sawAll=false,sawGep=false,sawSt=false,sawLd=false; for(auto &bb:*fn) for(auto &ins:bb){ auto op=std::string(ins.getOpcodeName()); if(op=="alloca") sawAll=true; else if(op=="getelementptr") sawGep=true; else if(op=="store") sawSt=true; else if(op=="load") sawLd=true; }
        assert(sawAll && sawGep && sawSt && sawLd);
    }
    // logic function module
    {
    std::cout << "IR test: starting logic module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_logic\" (fn :name \"logic\" :ret i32 :params [ (param i32 %x) (param i32 %y) ] :body [ (and %t i32 %x %y) (xor %u i32 %t %x) (ret i32 %u) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("logic"); assert(fn); bool saw=false; for(auto &bb:*fn) for(auto &ins:bb) if(ins.getOpcodeName()==std::string("xor")) saw=true; assert(saw);
    }
    // comparison function module
    {
    std::cout << "IR test: starting cmp module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_cmp\" (fn :name \"cmpf\" :ret i1 :params [ (param i32 %a) (param i32 %b) ] :body [ (lt %l i32 %a %b) (gt %g i32 %a %b) (eq %e i32 %a %b) (and %t i1 %l %g) (or %u i1 %t %e) (ret i1 %u) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("cmpf"); assert(fn); bool sawCmp=false; for(auto &bb:*fn) for(auto &ins:bb) if(ins.getOpcode()==llvm::Instruction::ICmp) sawCmp=true; assert(sawCmp);
    }
    // icmp predicate test (unsigned)
    {
        std::cout << "IR test: starting icmp predicate module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_icmp\" (fn :name \"uci\" :ret i1 :params [ (param u32 %a) (param u32 %b) ] :body [ (icmp %r u32 :pred ult %a %b) (ret i1 %r) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("uci"); assert(fn); size_t cmpCount=0; for(auto &bb:*fn) for(auto &ins:bb) if(ins.getOpcode()==llvm::Instruction::ICmp) ++cmpCount; assert(cmpCount==1);
    }
    // float arithmetic & fcmp test
    {
        std::cout << "IR test: starting float module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_float\" (fn :name \"fop\" :ret i1 :params [ (param f32 %x) (param f32 %y) ] :body [ (fadd %a f32 %x %y) (fmul %m f32 %a %x) (fcmp %c f32 :pred olt %m %y) (ret i1 %c) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("fop"); assert(fn); bool sawFAdd=false,sawFCmp=false; for(auto &bb:*fn) for(auto &ins:bb){ auto op=ins.getOpcodeName(); if(op==std::string("fadd")) sawFAdd=true; if(ins.getOpcode()==llvm::Instruction::FCmp) sawFCmp=true; } assert(sawFAdd && sawFCmp);
    }
    // mem function module
    {
    std::cout << "IR test: starting mem module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_mem\" (fn :name \"mem\" :ret i32 :params [] :body [ (alloca %p i32) (const %c i32 5) (store i32 %p %c) (load %v i32 %p) (ret i32 %v) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("mem"); assert(fn); bool sawAll=false,sawSt=false,sawLd=false; for(auto &bb:*fn) for(auto &ins:bb){ auto op=std::string(ins.getOpcodeName()); if(op=="alloca") sawAll=true; else if(op=="store") sawSt=true; else if(op=="load") sawLd=true; } assert(sawAll && sawSt && sawLd);
    }
    // if control flow module (simple if without else)
    {
    std::cout << "IR test: starting if module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_if\" (fn :name \"iff\" :ret i32 :params [] :body [ (const %c i1 1) (const %x i32 3) (const %y i32 4) (if %c [ (add %z i32 %x %y) ] ) (ret i32 %z) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn=m->getFunction("iff"); assert(fn); bool sawBr=false; for(auto &bb:*fn) for(auto &ins:bb){ if(ins.getOpcode()==llvm::Instruction::Br) sawBr=true; } assert(sawBr);
    }
    // if-else control flow module (branch chooses stored value via memory to avoid SSA merge)
    {
    std::cout << "IR test: starting if-else module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_ifelse\" (fn :name \"ife\" :ret i32 :params [] :body [ (alloca %p i32) (const %c i1 0) (const %x i32 10) (const %y i32 20) (if %c [ (store i32 %p %x) ] [ (store i32 %p %y) ]) (load %v i32 %p) (ret i32 %v) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn=m->getFunction("ife"); assert(fn); bool sawCond=false,sawStore=false; for(auto &bb:*fn) for(auto &ins:bb){ if(ins.getOpcode()==llvm::Instruction::Br) sawCond=true; if(ins.getOpcodeName()==std::string("store")) sawStore=true; } assert(sawCond && sawStore);
    }
    // while loop module
    {
    std::cout << "IR test: starting while module\n";
        IREmitter em(ctx);
    auto ast = parse("(module :id \"m_while\" (fn :name \"loop\" :ret i32 :params [] :body [ (alloca %p i32) (const %c i1 1) (const %init i32 1) (store i32 %p %init) (while %c [ (load %t i32 %p) (add %n i32 %t %t) (store i32 %p %n) (break) ]) (load %v i32 %p) (ret i32 %v) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn=m->getFunction("loop"); assert(fn); bool sawLoop=false; for(auto &bb:*fn) for(auto &ins:bb){ if(ins.getOpcode()==llvm::Instruction::Br) sawLoop=true; } assert(sawLoop);
    }
    // struct member access module
    {
    std::cout << "IR test: starting struct member module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_struct\" (struct :name MyS :fields [ (field :name a :type i32) (field :name b :type i32) ]) (fn :name \"sm\" :ret i32 :params [] :body [ (alloca %p (struct-ref MyS)) (member %v MyS %p a) (ret i32 %v) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("sm"); assert(fn); bool sawGEP=false,sawLoad=false; for(auto &bb:*fn) for(auto &ins:bb){ auto name=std::string(ins.getOpcodeName()); if(name=="getelementptr") sawGEP=true; else if(name=="load") sawLoad=true; } assert(sawGEP && sawLoad);
    }
    // struct member address (pointer) module
    {
    std::cout << "IR test: starting struct member-addr module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_struct_addr\" (struct :name MyS2 :fields [ (field :name a :type i32) (field :name b :type i32) ]) (fn :name \"sma\" :ret i32 :params [] :body [ (alloca %p (struct-ref MyS2)) (member-addr %pa MyS2 %p a) (const %c i32 42) (store i32 %pa %c) (member %v MyS2 %p a) (ret i32 %v) ]) )");
    TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("sma"); assert(fn);
    bool sawGEP=false,sawStore=false,sawLoad=false; 
    for(auto &bb:*fn) for(auto &ins:bb){ auto op=std::string(ins.getOpcodeName()); if(op=="getelementptr") sawGEP=true; else if(op=="store") sawStore=true; else if(op=="load") sawLoad=true; }
    assert(sawGEP && sawStore && sawLoad);
    }
    // global variables module
    {
    std::cout << "IR test: starting global module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_global\" (global :name G :type i32 :init 7) (fn :name \"useg\" :ret i32 :params [] :body [ (gload %v i32 G) (ret i32 %v) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m); auto fn = m->getFunction("useg"); assert(fn); bool sawLoad=false; for(auto &bb:*fn) for(auto &ins:bb) if(std::string(ins.getOpcodeName())=="load") sawLoad=true; assert(sawLoad); assert(m->getGlobalVariable("G")!=nullptr);
    }
    // function call module
    {
        std::cout << "IR test: starting call module\n";
        IREmitter em(ctx);
        auto ast = parse("(module :id \"m_call\" (fn :name \"callee\" :ret i32 :params [ (param i32 %x) (param i32 %y) ] :body [ (add %s i32 %x %y) (ret i32 %s) ]) (fn :name \"caller\" :ret i32 :params [] :body [ (const %a i32 5) (const %b i32 6) (call %r i32 callee %a %b) (ret i32 %r) ]) )");
        TypeCheckResult r; auto *m = em.emit(ast,r); assert(r.success && m);
    assert(m->getFunction("callee")!=nullptr); auto callerFn = m->getFunction("caller"); assert(callerFn); size_t callCount=0;
    for(auto &bb:*callerFn) for(auto &ins:bb) if(std::string(ins.getOpcodeName())=="call") ++callCount; assert(callCount==1);
    }
    std::cout << "IR emitter test passed\n";
}
