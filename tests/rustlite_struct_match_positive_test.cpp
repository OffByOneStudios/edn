#include <cassert>
#include <iostream>
#include <string>

#include "../languages/rustlite/parser/parser.hpp"
#include "../languages/rustlite/include/rustlite/expand.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

// Positive struct match arm pattern test.
// Provides a struct declaration Point { x, y } then matches a scrutinee value p.
// Ensures no E1456 (unknown field) / E1457 (duplicate field) diagnostics surface for valid arm.
int main(){
    const char* SRC = R"RL(fn use(){
        let p = 0;
        let r = match p { Point { x, y }{ 1 } _ { 2 } };
        return r;
    })RL";
    rustlite::Parser parser; auto pres = parser.parse_string(SRC, "structmatchpos.rl.rs"); if(!pres.success){ std::cerr<<"parse failed: "<<pres.error_message<<"\n"; return 1; }
    auto parsed = edn::parse(pres.edn); if(!parsed){ std::cerr<<"edn parse failed\n"; return 1; }
    // Inject struct declaration: (struct :name Point :fields [ (field :name x :type i32) (field :name y :type i32) ])
    using edn::node; using edn::node_ptr; using edn::list; using edn::vector_t; using edn::symbol; using edn::keyword;
    auto rl_make_sym = [](const std::string &s){ return std::make_shared<node>( node{ symbol{s}, {} } ); };
    auto rl_make_kw = [](const std::string &s){ return std::make_shared<node>( node{ keyword{s}, {} } ); };
    node_ptr structNode = std::make_shared<node>(); list sl; sl.elems.push_back(rl_make_sym("struct"));
    sl.elems.push_back(rl_make_kw("name")); sl.elems.push_back(rl_make_sym("Point"));
    sl.elems.push_back(rl_make_kw("fields")); vector_t fv;
    { list f; f.elems = { rl_make_sym("field"), rl_make_kw("name"), rl_make_sym("x"), rl_make_kw("type"), rl_make_sym("i32") }; fv.elems.push_back(std::make_shared<node>( node{ f, {} } )); }
    { list f; f.elems = { rl_make_sym("field"), rl_make_kw("name"), rl_make_sym("y"), rl_make_kw("type"), rl_make_sym("i32") }; fv.elems.push_back(std::make_shared<node>( node{ f, {} } )); }
    sl.elems.push_back(std::make_shared<node>( node{ fv, {} } )); structNode->data = sl;
    if(!std::holds_alternative<edn::list>(parsed->data)){ std::cerr<<"expected list module"; return 1; }
    auto &ml = std::get<edn::list>(parsed->data).elems; if(ml.empty() || !std::holds_alternative<edn::symbol>(ml[0]->data)){ std::cerr<<"expected module head"; return 1; }
    ml.insert(ml.begin()+1, structNode);
    auto expanded = rustlite::expand_rustlite(parsed); if(!expanded){ std::cerr<<"expand failed\n"; return 1; }
    edn::TypeContext tctx; edn::TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    for(auto &e : tcres.errors){ if(e.code=="E1456"||e.code=="E1457"){ std::cerr<<"unexpected struct pattern diagnostic: "<<e.code<<" "<<e.message<<"\n"; return 1; } }
    std::cout<<"[rustlite-struct-match-positive] ok\n"; return 0;
}
