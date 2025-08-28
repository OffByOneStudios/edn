#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"
using namespace edn;
int main(){
  const char* edn =
    "(module :id \"ematch_nonex\" "
    "  (enum :name Opt2 :variants [ (None) (Some i32) ]) "
    "  (rfn :name \"use\" :ret i32 :params [ ] :body [ "
    "    (const %zero i32 0) "
    "    (enum-ctor %o Opt2 None) "
    "    (ematch %out i32 Opt2 %o :arms [ (arm None :body [ :value %zero ]) ]) "
    "    (ret i32 %out) "
    "  ]) "
    ")"; // non-exhaustive; default inserted (panic path) but not executed here
  auto ast = parse(edn);
  auto expanded = rustlite::expand_rustlite(ast);
  TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
  bool sawE1600=false; for(auto &e: res.errors){ if(e.code=="E1600") sawE1600=true; }
  if(!sawE1600){
    std::cerr << "EXPECTED E1600 diagnostic not found\n";
    for(auto &e:res.errors) std::cerr<<e.code<<":"<<e.message<<"\n";
    return 1; // signal failure if diagnostic missing
  }
  // We intentionally stop before IR emit because type check failed.
  std::cout << "[ematch-non-exhaustive] produced E1600 as expected\n"; return 0; }
