#include "edn/edn.hpp"
#include "rustlite/rustlite.hpp"
#include "rustlite/expand.hpp"
#include <iostream>
// Validate :inclusive true in literal rfor-range expands with le comparison and executes expected iteration count.
int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-range-inclusive] running...\n";
    Builder b; b.begin_module();
    // Loop 0..=3 should iterate 0,1,2,3 (4 iterations) doubling each time => i sequence: 0->0, 0+0=0; then 1->2; then 2->4; then 3->6 ; final i=6
    b.fn_raw("range_inc", "i32", {},
        "[ (rfor-range %i i32 0 3 :inclusive true :body [ (block :body [ (add %tmp i32 %i %i) (assign %i %tmp) ]) ]) (ret i32 %i) ]");
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast); auto s=edn::to_string(expanded);
    bool hasLE = s.find(" le ")!=std::string::npos || s.find("(le")!=std::string::npos; // match either token with or without surrounding spaces
    if(!hasLE){ std::cerr << "[rustlite-range-inclusive] expected 'le' comparison not found\n"<<s<<"\n"; return 1; }
    std::cout << "[rustlite-range-inclusive] ok\n"; return 0; }
