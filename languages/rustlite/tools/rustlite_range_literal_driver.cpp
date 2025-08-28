// Driver to exercise rrange + rfor-range (tuple form)
#include "edn/edn.hpp"
#include "rustlite/rustlite.hpp"
#include "rustlite/expand.hpp"
#include <iostream>

int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-range-lit] running...\n";
    Builder b; b.begin_module();
    // Literal range creation + loop via tuple form
    b.fn_raw("range_demo", "i32", {},
        "[ (rrange %r i32 0 4 :inclusive false) (rfor-range %i i32 %r :body [ (block :body [ (add %tmp i32 %i %i) (assign %i %tmp) ]) ]) (ret i32 %i) ]");
    auto prog = b.build(); auto ast = parse(prog.edn_text); auto expanded = rustlite::expand_rustlite(ast);
    auto s = edn::to_string(expanded);
    // Range tuple currently materializes as a struct literal for __Tuple3
    bool hasRangeTuple = s.find("struct-lit")!=std::string::npos && s.find("__Tuple")!=std::string::npos;
    bool lowered = s.find("(for")!=std::string::npos;
    if(!hasRangeTuple || !lowered){
        std::cerr << "[rustlite-range-lit] missing expected constructs hasRangeTuple="<<hasRangeTuple<<" lowered="<<lowered<<"\n"<<s<<"\n"; return 1; }
    std::cout << "[rustlite-range-lit] ok\n"; return 0;
}
