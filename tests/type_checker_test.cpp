// Type checker tests
#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/types.hpp"
#include "edn/type_check.hpp"
#include "edn/type_check.inl"

using namespace edn;

void run_type_checker_tests(){
    TypeContext ctx;
    TypeChecker tc(ctx);
    auto good = parse("(module :id \"m\" (fn :name \"f\" :ret i32 :params [] :body []))");
    auto r1 = tc.check_module(good);
    assert(r1.success);
    auto bad = parse("(module (fn :ret i32 :params [] :body []))");
    auto r2 = tc.check_module(bad);
    assert(!r2.success && !r2.errors.empty());
    auto good_struct = parse("(module (struct :name Point :fields [ (:name x :type i32) (:name y :type i32) ]) (fn :name \"getx\" :ret i32 :params [(param (ptr (struct-ref Point)) %p)] :body [ (member %mx Point %p x) (ret i32 %mx) ]))");
    auto rs = tc.check_module(good_struct); assert(rs.success);
    auto bad_member = parse("(module (struct :name Point :fields [ (:name x :type i32) ]) (fn :name \"gety\" :ret i32 :params [(param (ptr (struct-ref Point)) %p)] :body [ (member %my Point %p y) (ret i32 %my) ]))");
    auto rbadm = tc.check_module(bad_member); assert(!rbadm.success);
    // load/store/index + metadata
    auto mem = parse("(module (struct :name S :fields [ (:name v :type i32) ]) (fn :name \"memops\" :ret i32 :params [ (param (ptr (struct-ref S)) %s) ] :body [ (member-addr %ma S %s v) (const %c i32 0) (store i32 %ma %c) (load %lv i32 %ma) (ret i32 %lv) ]))");
    auto rm = tc.check_module(mem); assert(rm.success);
    auto arr = parse("(module (fn :name \"arr\" :ret i32 :params [] :body [ (const %zero i32 0) (const %zero2 i32 0) (const %a (array :elem i32 :size 4) 0) (index %e i32 %a %zero) (ret i32 %zero) ]))");
    auto ra = tc.check_module(arr); assert(!ra.success); // expect failure: %a not pointer to array
    auto callm = parse("(module (fn :name \"callee\" :ret i32 :params [] :body [ (const %c i32 0) (ret i32 %c) ]) (fn :name \"caller\" :ret i32 :params [] :body [ (call %r i32 callee) (ret i32 %r) ]))");
    auto rc = tc.check_module(callm); assert(rc.success);
    auto badcall = parse("(module (fn :name \"callee\" :ret i32 :params [] :body [ (const %c i32 0) (ret i32 %c) ]) (fn :name \"caller\" :ret i32 :params [] :body [ (call %r i64 callee) (ret i32 %r) ]))");
    auto rbc = tc.check_module(badcall); assert(!rbc.success);
    auto allocm = parse("(module (fn :name \"alloc\" :ret i32 :params [] :body [ (alloca %p i32) (ret i32 %p) ]))");
    auto ral = tc.check_module(allocm); assert(!ral.success); // returning pointer as i32 mismatch
    // block + locals + assign
    auto blockm = parse("(module (fn :name \"blk\" :ret i32 :params [] :body [ (const %c i32 1) (block :locals [ (local i32 %x) ] :body [ (assign %x %c) ]) (ret i32 %c) ]))");
    auto rblock = tc.check_module(blockm); assert(rblock.success);
    auto badassign = parse("(module (fn :name \"blk2\" :ret i32 :params [] :body [ (const %c i32 1) (assign %x %c) (ret i32 %c) ]))");
    auto rbadassign = tc.check_module(badassign); assert(!rbadassign.success);
    // call arg checking
    auto call2 = parse("(module (fn :name \"add2\" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [ (add %s i32 %a %b) (ret i32 %s) ]) (fn :name \"use\" :ret i32 :params [] :body [ (const %c i32 0) (call %r i32 add2 %c %c) (ret i32 %r) ]))");
    auto rcall2 = tc.check_module(call2); assert(rcall2.success);
    auto badcall2 = parse("(module (fn :name \"add2\" :ret i32 :params [ (param i32 %a) (param i32 %b) ] :body [ (add %s i32 %a %b) (ret i32 %s) ]) (fn :name \"use\" :ret i32 :params [] :body [ (const %c i32 0) (call %r i32 add2 %c) (ret i32 %r) ]))");
    auto rbadcall2 = tc.check_module(badcall2); assert(!rbadcall2.success);
    // if / while control flow
    auto ctrl = parse("(module (fn :name \"ctrl\" :ret i32 :params [] :body [ (const %t i1 1) (if %t [ (const %a i32 1) ] [ (const %b i32 2) ]) (while %t [ (const %c i32 3) ]) (const %zero i32 0) (ret i32 %zero) ]))");
    auto rctrl = tc.check_module(ctrl); assert(rctrl.success);
    auto badif = parse("(module (fn :name \"badif\" :ret i32 :params [] :body [ (const %x i32 1) (if %x [ (const %a i32 1) ]) (ret i32 %x) ]))");
    auto rbadif = tc.check_module(badif); assert(!rbadif.success);
    // logical / bitwise
    auto bitok = parse("(module (fn :name \"bit\" :ret i32 :params [] :body [ (const %a i32 1) (const %b i32 2) (and %c i32 %a %b) (or %d i32 %a %b) (xor %e i32 %a %b) (shl %f i32 %a %b) (lshr %g i32 %a %b) (ashr %h i32 %a %b) (ret i32 %c) ]))");
    auto rbitok = tc.check_module(bitok); assert(rbitok.success);
    auto bitbad = parse("(module (fn :name \"bitbad\" :ret i32 :params [] :body [ (const %a f32 1) (const %b f32 2) (and %c f32 %a %b) (ret i32 %c) ]))");
    auto rbitbad = tc.check_module(bitbad); assert(!rbitbad.success);
    // break usage
    auto breakbad = parse("(module (fn :name \"bb\" :ret i32 :params [] :body [ (break) (const %z i32 0) (ret i32 %z) ]))");
    auto rbreakbad = tc.check_module(breakbad); assert(!rbreakbad.success);
    auto breakok = parse("(module (fn :name \"bo\" :ret i32 :params [] :body [ (const %t i1 1) (while %t [ (break) ]) (const %z i32 0) (ret i32 %z) ]))");
    auto rbreakok = tc.check_module(breakok); assert(rbreakok.success);
    // comparisons
    auto cmpok = parse("(module (fn :name \"cmp\" :ret i1 :params [ (param i32 %a) (param i32 %b) ] :body [ (lt %l i32 %a %b) (ret i1 %l) ]))");
    auto rcmpok = tc.check_module(cmpok); assert(rcmpok.success);
    // Legacy cmp warning path (cannot assert on env presence here, just ensure still succeeds)
    auto cmpwarn = parse("(module (fn :name \"cmpw\" :ret i1 :params [ (param i32 %a) (param i32 %b) ] :body [ (gt %g i32 %a %b) (ret i1 %g) ]))");
    auto rcmpwarn = tc.check_module(cmpwarn); assert(rcmpwarn.success);
    auto cmpbad = parse("(module (fn :name \"cmpb\" :ret i1 :params [ (param f32 %a) (param f32 %b) ] :body [ (lt %l f32 %a %b) (ret i1 %l) ]))");
    auto rcmpbad = tc.check_module(cmpbad); assert(!rcmpbad.success);
    // unsigned arithmetic & icmp
    auto uarith = parse("(module (fn :name \"uarith\" :ret u32 :params [ (param u32 %a) (param u32 %b) ] :body [ (udiv %d u32 %a %b) (urem %r u32 %a %b) (add %s u32 %d %r) (ret u32 %s) ]))");
    auto ruarith = tc.check_module(uarith); assert(ruarith.success);
    auto icmpu = parse("(module (fn :name \"icmpu\" :ret i1 :params [ (param u32 %a) (param u32 %b) ] :body [ (icmp %r u32 :pred ult %a %b) (ret i1 %r) ]))");
    auto ricmpu = tc.check_module(icmpu); assert(ricmpu.success);
    auto badicmp = parse("(module (fn :name \"badicmp\" :ret i1 :params [ (param f32 %a) (param f32 %b) ] :body [ (icmp %r f32 :pred ult %a %b) (ret i1 %r) ]))");
    auto rbadicmp = tc.check_module(badicmp); assert(!rbadicmp.success);
    // float arithmetic
    auto floatok = parse("(module (fn :name \"fops\" :ret f32 :params [ (param f32 %x) (param f32 %y) ] :body [ (fadd %a f32 %x %y) (fmul %m f32 %a %x) (ret f32 %m) ]))");
    auto rfloatok = tc.check_module(floatok); assert(rfloatok.success);
    auto fcmpok = parse("(module (fn :name \"fc\" :ret i1 :params [ (param f32 %x) (param f32 %y) ] :body [ (fcmp %c f32 :pred oeq %x %y) (ret i1 %c) ]))");
    auto rfcmpok = tc.check_module(fcmpok); assert(rfcmpok.success);
    auto fcmpbad = parse("(module (fn :name \"fcbad\" :ret i1 :params [ (param f32 %x) (param f32 %y) ] :body [ (fcmp %c f32 :pred ult %x %y) (ret i1 %c) ]))");
    auto rfcmpbad = tc.check_module(fcmpbad); assert(!rfcmpbad.success);
    // globals
    auto globok = parse("(module (global :name G :type i32 :init 5) (fn :name \"useg\" :ret i32 :params [] :body [ (gload %v i32 G) (ret i32 %v) ]))");
    auto rglobok = tc.check_module(globok); assert(rglobok.success);
    auto globbad = parse("(module (global :name G :type i32 :init 5) (fn :name \"useg\" :ret i32 :params [] :body [ (gload %v i64 G) (ret i32 %v) ]))");
    auto rglobbad = tc.check_module(globbad); assert(!rglobbad.success);
    auto gstorebad = parse("(module (global :name G :type i32 :init 5) (fn :name \"useg\" :ret i32 :params [] :body [ (const %c i64 1) (gstore i32 G %c) (ret i32 %c) ]))");
    auto rgstorebad = tc.check_module(gstorebad); assert(!rgstorebad.success);
    // member-addr misuse (base not pointer)
    auto membadd = parse("(module (struct :name S :fields [ (:name a :type i32) ]) (fn :name \"badma\" :ret i32 :params [ (param (struct-ref S) %s) ] :body [ (member-addr %pa S %s a) (ret i32 %s) ]))");
    auto rmembadd = tc.check_module(membadd); assert(!rmembadd.success);
    std::cout << "Added extended negative tests passed\n";
    std::cout << "Type checker tests passed\n";
}
