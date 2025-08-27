// Negative tests for rcstr / rbytes -> cstr / bytes core ops.
#include <iostream>
#include <string>
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

// We deliberately craft several malformed forms and ensure the expected error codes appear.
// Target error codes (see type_check.inl):
// cstr:  E1500 arity, E1501 dst, E1503 literal must be symbol, E1504 malformed quotes
// bytes: E1510 arity, E1513 expects vector, E1514 empty, E1515 element not int, E1516 out of range
// (We skip E1502/E1512 duplicate dst because that requires crafting two identical dst ops; not needed for smoke.)

int main(){
    using namespace edn; using namespace rustlite;
    std::cout << "[rustlite-literals-neg] building demo...\n";
    // Construct one function body containing all malformed instructions so they are all type-checked in one pass.
    const char* edn_text =
        "(module :id \"rl_lits_neg\" "
        "  (rfn :name \"bad_lits\" :ret i32 :params [ ] :body [ "
        // cstr arity (missing literal)
        "    (cstr %a) "
        // cstr bad dst (no %)
        "    (cstr a \"ok\") "
        // cstr literal not symbol (supply vector instead)
        "    (cstr %b [ 1 2 ]) "
        // cstr malformed quotes (symbol not wrapped properly)
        "    (cstr %c notquoted) "
        // bytes arity (missing vector)
        "    (bytes %d) "
        // bytes expects vector (give symbol)
        "    (bytes %e foo) "
        // bytes empty vector
        "    (bytes %f [ ]) "
        // bytes element not int (string symbol)
        "    (bytes %g [ 1 two 3 ]) "
        // bytes element out of range
        "    (bytes %h [ 0 256 ]) "
        "    (const %z i32 0) (ret i32 %z) "
        "  ]) "
        ")";

    auto ast = parse(edn_text);
    auto expanded = expand_rustlite(ast); // rcstr/rbytes not used directly here; we emit core ops directly
    TypeContext tctx; TypeChecker tc(tctx);
    auto tcres = tc.check_module(expanded);
    if(tcres.success){
        std::cerr << "[rustlite-literals-neg] expected failures but type check passed\n";
        return 1;
    }
    // Track presence of each targeted error code
    bool saw1500=false,saw1501=false,saw1503=false,saw1504=false;
    bool saw1510=false,saw1513=false,saw1514=false,saw1515=false,saw1516=false;
    for(const auto &e : tcres.errors){
        if(e.code=="E1500") saw1500=true; if(e.code=="E1501") saw1501=true; if(e.code=="E1503") saw1503=true; if(e.code=="E1504") saw1504=true;
        if(e.code=="E1510") saw1510=true; if(e.code=="E1513") saw1513=true; if(e.code=="E1514") saw1514=true; if(e.code=="E1515") saw1515=true; if(e.code=="E1516") saw1516=true;
    }
    int missing = 0;
    auto req = [&](bool cond, const char* name){ if(!cond){ std::cerr << "[rustlite-literals-neg] missing expected error " << name << "\n"; ++missing; } };
    req(saw1500,"E1500"); req(saw1501,"E1501"); req(saw1503,"E1503"); req(saw1504,"E1504");
    req(saw1510,"E1510"); req(saw1513,"E1513"); req(saw1514,"E1514"); req(saw1515,"E1515"); req(saw1516,"E1516");
    if(missing){ return 2; }
    std::cout << "[rustlite-literals-neg] saw expected errors\n";
    return 0;
}
