#include "edn/edn.hpp"
#include "rustlite/rustlite.hpp" // Builder
#include "rustlite/expand.hpp"   // expand_rustlite
#include <iostream>
#include <sstream>

int main(){
    using namespace rustlite;
    std::cout << "[rustlite-range-for] running...\n";
    Builder b;
    b.begin_module();
    b.fn_raw("range_sum", "i32", {},
        "[ (rfor-range %i i32 0 5 :body [ (block :body [ (add %tmp i32 %i %i) (assign %i %tmp) ]) ]) (ret i32 %i) ]");
    auto prog = b.build();
    auto ast = edn::parse(prog.edn_text.c_str());
    auto expanded = rustlite::expand_rustlite(ast);
    // Materialize expanded module to string (shared_ptr stream insertion prints address otherwise)
    auto s = edn::to_string(expanded);
    bool stillMacro = s.find("(rfor-range") != std::string::npos;
    bool hasFor = s.find("(for") != std::string::npos; // look for for loop op
    bool hasAdd = s.find("(add") != std::string::npos;
    if(stillMacro || !hasFor || !hasAdd){
        std::cerr << "[rustlite-range-for] failure: stillMacro="<<stillMacro
                  << " hasFor="<<hasFor << " hasAdd="<<hasAdd << "\n--- Expanded Module ---\n" << s << "\n-----------------------\n";
        return 1;
    }
    std::cout << "[rustlite-range-for] ok\n";
    return 0;
}
