#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

// Legacy non-exhaustive rmatch should yield E1415 (not E1600 which is reserved for ematch metadata path).
using namespace edn;
int main(){
  std::cout << "[rustlite-rmatch-non-exhaustive-legacy] starting...\n";
  const char* edn =
    "(module :id \"rmatch_nonex_legacy\" "
    "  (renum :name Tri2 :variants [ A B C ]) "
    "  (rfn :name \"pick\" :ret i32 :params [ ] :body [ "
    "    (const %zero i32 0) (const %one i32 1) "
    "    (sum-new %v Tri2 A []) "
    // Construct core match directly (bypasses rmatch macro) to simulate legacy missing default scenario.
    "    (match %out i32 Tri2 %v :cases [ (case A :body [ (as %out i32 %zero) ]) (case B :body [ (as %out i32 %one) ]) ]) "
    "    (ret i32 %out) "
    "  ]) "
    ")"; // Missing C and no :default => expect E1415
  auto ast = parse(edn);
  auto expanded = rustlite::expand_rustlite(ast); // rmatch expansion (legacy)
  TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
  bool sawE1415=false; bool sawE1600=false;
  for(auto &e: res.errors){ if(e.code=="E1415") sawE1415=true; if(e.code=="E1600") sawE1600=true; }
  if(!sawE1415 || sawE1600){
    std::cerr << "Legacy non-exhaustive core match diagnostics unexpected. want E1415 only. got:\n";
    for(auto &e:res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1; }
  std::cout << "[rustlite-rmatch-non-exhaustive-legacy] produced E1415 as expected\n";
  return 0; }
