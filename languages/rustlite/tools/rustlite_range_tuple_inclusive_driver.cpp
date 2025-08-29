// Driver to exercise rfor-range tuple inclusive semantics
#include "edn/edn.hpp"
#include "rustlite/rustlite.hpp"
#include "rustlite/expand.hpp"
#include <iostream>

int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-range-tuple-inclusive] running...\n";
    Builder b; b.begin_module();
    // Build a range tuple with inclusive flag true (0..=3) and sum loop var accumulation
    b.fn_raw("range_tuple_inclusive_demo", "i32", {},
        "[ (rrange %r i32 0 3 :inclusive true) (rfor-range %i i32 %r :body [ (block :body [ (add %tmp i32 %i %i) (assign %i %tmp) ]) ]) (ret i32 %i) ]");
    auto prog = b.build(); auto ast = parse(prog.edn_text); auto expanded = rustlite::expand_rustlite(ast);
    auto s = edn::to_string(expanded);
    // Expect inclusive logic: presence of 'or' combining lt + and(inclusive, eq)
    bool hasOr = s.find("(or")!=std::string::npos;
    bool hasAnd = s.find("(and")!=std::string::npos;
    if(!hasOr || !hasAnd){
        std::cerr << "[rustlite-range-tuple-inclusive] missing inclusive condition logic\n" << s << "\n"; return 1;
    }
    std::cout << "[rustlite-range-tuple-inclusive] ok\n"; return 0;
}
