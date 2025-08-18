#include <cassert>
#include <iostream>
#include "../parser/parser.hpp"

int main(){
    rustlite::Parser p;
    {
        auto r = p.parse_string("  // empty module\n\n", "mem");
        assert(r.success);
        assert(r.edn.find("(module") != std::string::npos);
    }
    {
        const char* src = R"(// one empty function
fn main() {
    // empty body
}
)";
        auto r = p.parse_string(src, "mem2");
        assert(r.success);
    }
    std::cout << "parser smoke ok\n";
    return 0;
}
