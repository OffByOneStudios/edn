#include <iostream>
#include <cstdlib>
#include "rustlite/expand.hpp"
#include "rustlite/macros/context.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "edn/ir_emitter.hpp"
#include "rustlite/features.hpp"

using namespace edn;
using namespace rustlite;

static const char* MODULE_OK = R"EDN(
(module :id "bounds_ok"
  (rfn :name "in_bounds" :ret i32 :params [] :body [
    (arr %a i32 [ (const %v0 i32 10) (const %v1 i32 20) (const %v2 i32 30) ])
    (const %LEN i32 3)
    (const %i1 i32 1)
    (rindex-load %x i32 %a %i1 :len %LEN)
    (ret i32 %x)
  ])
)
)EDN";

static const char* MODULE_OOB = R"EDN(
(module :id "bounds_oob"
  (rfn :name "oob" :ret i32 :params [] :body [
    (arr %a i32 [ (const %v0 i32 1) (const %v1 i32 2) ])
    (const %i2 i32 2) ; equal to length -> OOB
    (rindex-load %x i32 %a %i2)
    (ret i32 %x)
  ])
)
)EDN";

int main(){
    std::cout << "[rustlite-bounds] running...\n";
    setenv("RUSTLITE_BOUNDS","1",1);
  auto okAst = parse(MODULE_OK);
  auto oobAst = parse(MODULE_OOB);
  auto okExpanded = rustlite::expand_rustlite(okAst);
  auto oobExpanded = rustlite::expand_rustlite(oobAst);
  std::function<void(const edn::node_ptr&, std::ostream&)> dump = [&](const edn::node_ptr& n, std::ostream& os){
    if(!n){ os << "nil"; return; }
    if(std::holds_alternative<edn::symbol>(n->data)){ os << std::get<edn::symbol>(n->data).name; return; }
    if(std::holds_alternative<edn::keyword>(n->data)){ os << ":" << std::get<edn::keyword>(n->data).name; return; }
    if(std::holds_alternative<int64_t>(n->data)){ os << std::get<int64_t>(n->data); return; }
    if(std::holds_alternative<std::string>(n->data)){ os << '"' << std::get<std::string>(n->data) << '"'; return; }
    if(std::holds_alternative<bool>(n->data)){ os << (std::get<bool>(n->data)?"true":"false"); return; }
    if(std::holds_alternative<edn::list>(n->data)){ os << '('; bool f=true; for(auto &c: std::get<edn::list>(n->data).elems){ if(!f) os<<' '; dump(c, os); f=false;} os << ')'; return; }
    if(std::holds_alternative<edn::vector_t>(n->data)){ os << '['; bool f=true; for(auto &c: std::get<edn::vector_t>(n->data).elems){ if(!f) os<<' '; dump(c, os); f=false;} os << ']'; return; }
  };
  {
    std::ostringstream oss; dump(okExpanded, oss); std::cout << oss.str() << "\n"; }
  {
    std::ostringstream oss; dump(oobExpanded, oss); std::cout << oss.str() << "\n"; }
    std::cout << "[rustlite-bounds] done\n";
    return 0;
}
