#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"
using namespace edn;
int main(){
  const char* edn =
    "(module :id \"ematch_ex\" "
    " (enum :name Tri :variants [ (A) (B) (C) ]) "
    " (rfn :name \"pick\" :ret i32 :params [ (param i32 %x) ] :body [ "
    "   (const %zero i32 0) (const %one i32 1) (const %two i32 2) "
    "   (enum-ctor %t Tri A) "
    "   (ematch %out i32 Tri %t :arms [ (arm A :body [ :value %zero ]) (arm B :body [ :value %one ]) (arm C :body [ :value %two ]) ]) "
    "   (ret i32 %out) "
    " ]) )";
  auto ast = parse(edn);
  auto expanded = rustlite::expand_rustlite(ast);
  TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
  if(!res.success){ for(auto &e:res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
  IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
  std::cout << "[ematch-exhaustive] ok\n"; return 0; }
