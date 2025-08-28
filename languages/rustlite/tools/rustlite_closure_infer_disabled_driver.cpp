#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-closure-infer-disabled] running...\n";
    Builder b; b.begin_module();
    b.fn_raw("adder", "i32", { {"env","i32"}, {"x","i32"} }, "[ (add %r i32 %env %x) (ret i32 %r) ]");
    b.fn_raw("make", "i32", {}, "[ (const %cap i32 9) (rclosure %c adder) (ret i32 %cap) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    auto s = edn::to_string(expanded);
    // When inference disabled, rclosure without :captures should remain and later macro pass will likely fail to lower; we just assert absence of make-closure.
    bool stillRClosure = s.find("(rclosure")!=std::string::npos;
    bool hasMakeClosure = s.find("make-closure")!=std::string::npos;
    if(!stillRClosure || hasMakeClosure){
        std::cerr << "[rustlite-closure-infer-disabled] unexpected transformation stillRClosure="<<stillRClosure<<" hasMake="<<hasMakeClosure<<"\n"<<s<<"\n"; return 1; }
    std::cout << "[rustlite-closure-infer-disabled] ok\n"; return 0;
}
