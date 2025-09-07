#include <cassert>
#include <iostream>
#include <string>

#include "../languages/rustlite/parser/parser.hpp"
#include "../languages/rustlite/include/rustlite/expand.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

// Negative struct match arm pattern test: references unknown field 'z' in Point { x, z }.
int main(){
    const char* SRC = R"RL(fn use(){
        let p = 0;
        let r = match p { Point { x, z }{ 1 } _ { 2 } };
        return r;
    })RL";
    rustlite::Parser parser; auto pres = parser.parse_string(SRC, "structmatchunk.rl.rs"); if(!pres.success){ std::cerr<<"parse failed: "<<pres.error_message<<"\n"; return 1; }
    auto parsed = edn::parse(pres.edn); if(!parsed){ std::cerr<<"edn parse failed fn\n"; return 1; }
    using edn::vector_t; using edn::node; using edn::node_ptr; using edn::list; using edn::symbol; using edn::keyword;
    // Local symbol/keyword constructors
    auto rl_make_sym = [](const std::string &s){ return std::make_shared<node>( node{ symbol{s}, {} } ); };
    auto rl_make_kw = [](const std::string &s){ return std::make_shared<node>( node{ keyword{s}, {} } ); };
    node_ptr structNode = std::make_shared<node>(); list sl; sl.elems.push_back(rl_make_sym("struct"));
    sl.elems.push_back(rl_make_kw("name")); sl.elems.push_back(rl_make_sym("Point"));
    sl.elems.push_back(rl_make_kw("fields")); vector_t fieldsV; {
        list f; f.elems = { rl_make_sym("field"), rl_make_kw("name"), rl_make_sym("x"), rl_make_kw("type"), rl_make_sym("i32") }; fieldsV.elems.push_back(std::make_shared<node>( node{ f, {} } )); }
    { list f; f.elems = { rl_make_sym("field"), rl_make_kw("name"), rl_make_sym("y"), rl_make_kw("type"), rl_make_sym("i32") }; fieldsV.elems.push_back(std::make_shared<node>( node{ f, {} } )); }
    sl.elems.push_back(std::make_shared<node>( node{ fieldsV, {} } )); structNode->data = sl;
    auto &ml = std::get<edn::list>(parsed->data).elems; ml.insert(ml.begin()+1, structNode);
    auto expanded = rustlite::expand_rustlite(parsed); if(!expanded){ std::cerr<<"expand failed\n"; return 1; }
    edn::TypeContext tctx; edn::TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    bool saw=false; for(auto &e : tcres.errors){ if(e.code=="E1456"){ saw=true; break; } }
    if(!saw){ std::cerr<<"expected E1456 unknown field diagnostic; errors=\n"; for(auto &e: tcres.errors){ std::cerr<<e.code<<": "<<e.message<<"\n"; } std::cerr<<edn::to_string(expanded)<<"\n"; return 1; }
    std::cout<<"[rustlite-struct-match-unknown-field-neg] ok\n"; return 0;
}
