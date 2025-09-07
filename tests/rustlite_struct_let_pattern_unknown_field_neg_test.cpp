// Unknown field negative test: struct declared with only x,y but pattern references z.
#include <cassert>
#include <iostream>
#include <string>

#include "../languages/rustlite/parser/parser.hpp"
#include "../languages/rustlite/include/rustlite/expand.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"

int main(){
    const char* SRC = R"RL(fn use(){
        let p = 0;
        let Point { x, z } = p; // 'z' unknown
        return 0;
    })RL";
    rustlite::Parser parser; auto pres = parser.parse_string(SRC, "structpatunk.rl.rs"); if(!pres.success){ std::cerr<<"parse failed: "<<pres.error_message<<"\n"; return 1; }
    auto parsed = edn::parse(pres.edn); if(!parsed){ std::cerr<<"edn parse failed fn\n"; return 1; }
    using edn::vector_t; using edn::node; using edn::node_ptr; using edn::list; using edn::symbol; using edn::keyword;
    // Local symbol/keyword constructors (avoid dependency on helpers header)
    auto rl_make_sym = [](const std::string &s){ return std::make_shared<node>( node{ symbol{s}, {} } ); };
    auto rl_make_kw = [](const std::string &s){ return std::make_shared<node>( node{ keyword{s}, {} } ); };
    // Build keyword form struct node: (struct :name Point :fields [ (field :name x :type i32) (field :name y :type i32) ])
    node_ptr structNode = std::make_shared<node>(); list sl; sl.elems.push_back(rl_make_sym("struct"));
    sl.elems.push_back(rl_make_kw("name")); sl.elems.push_back(rl_make_sym("Point"));
    sl.elems.push_back(rl_make_kw("fields"));
    vector_t fieldsV;
    { list f; f.elems = { rl_make_sym("field"), rl_make_kw("name"), rl_make_sym("x"), rl_make_kw("type"), rl_make_sym("i32") }; fieldsV.elems.push_back(std::make_shared<node>( node{ f, {} } )); }
    { list f; f.elems = { rl_make_sym("field"), rl_make_kw("name"), rl_make_sym("y"), rl_make_kw("type"), rl_make_sym("i32") }; fieldsV.elems.push_back(std::make_shared<node>( node{ f, {} } )); }
    sl.elems.push_back(std::make_shared<node>( node{ fieldsV, {} } )); structNode->data = sl;
    if(!std::holds_alternative<edn::list>(parsed->data)){ std::cerr<<"expected list module\n"; return 1; }
    auto &ml = std::get<edn::list>(parsed->data).elems; if(ml.empty() || !std::holds_alternative<edn::symbol>(ml[0]->data) || std::get<edn::symbol>(ml[0]->data).name!="module"){ std::cerr<<"expected module head\n"; return 1; }
    ml.insert(ml.begin()+1, structNode);
    auto expanded = rustlite::expand_rustlite(parsed); if(!expanded){ std::cerr<<"expand failed\n"; return 1; }
    edn::TypeContext tctx; edn::TypeChecker tc(tctx); auto tcres = tc.check_module(expanded);
    bool saw=false; for(auto &e : tcres.errors){ if(e.code=="E1456"){ saw=true; break; } }
    if(!saw){ std::cerr<<"expected E1456 unknown field diagnostic via TypeChecker errors; errors=\n"; for(auto &e: tcres.errors){ std::cerr<<e.code<<": "<<e.message<<"\n"; } std::cerr<<edn::to_string(expanded)<<"\n"; return 1; }
    std::cout << "[rustlite-struct-let-pattern-unknown-field-neg] ok\n"; return 0;
}
