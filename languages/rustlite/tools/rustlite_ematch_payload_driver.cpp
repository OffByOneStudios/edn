#include <iostream>
#include <cassert>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/expand.hpp"

using namespace edn;

int main(){
  std::cout << "[rustlite-ematch-payload] binding demo...\n";
  // Enum with a payload-carrying variant; bind both fields and compute a sum.
  const char* edn =
    "(module :id \"ematch_payload\" "
    "  (enum :name PairOrUnit :variants [ (Unit) (Pair i32 i32) ]) "
    "  (rfn :name \"sum_pair\" :ret i32 :params [ ] :body [ "
    "    (const %two i32 2) (const %five i32 5) "
    "    (enum-ctor %v PairOrUnit Pair %two %five) "
    "    (ematch %out i32 PairOrUnit %v :arms [ "
    "       (arm Unit :body [ (const %z i32 0) :value %z ]) "
    "       (arm Pair :binds [ %a %b ] :body [ (add %s i32 %a %b) :value %s ]) "
    "    ]) "
    "    (ret i32 %out) "
    "  ]) "
    ")";
  auto ast = parse(edn);
  auto expanded = rustlite::expand_rustlite(ast);
  TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
  if(!res.success){
    for(auto &e:res.errors) std::cerr<<e.code<<":"<<e.message<<"\n"; return 1;
  }
  IREmitter em(tctx); TypeCheckResult ir; auto *mod = em.emit(expanded, ir); assert(mod && ir.success);
  std::cout << "[rustlite-ematch-payload] ok\n"; return 0;
}
