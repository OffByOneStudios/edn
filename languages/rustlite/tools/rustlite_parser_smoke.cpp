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
        const char* src = R"(
fn main(){
    foo(1, x, -2);
}
)";
        auto r = p.parse_string(src, "mem3");
        assert(r.success);
        // Should include an rcall form
        assert(r.edn.find("(rcall ") != std::string::npos);
    }
    {
        const char* src = R"(// one empty function
fn main() {
    // empty body
}
)";
        auto r = p.parse_string(src, "mem2");
        assert(r.success);
    // Ensure the function name is present in the lowered EDN
    assert(r.edn.find("\"main\"") != std::string::npos);
    assert(r.edn.find("(fn ") != std::string::npos);
    }
    std::cout << "parser smoke ok\n";
    return 0;
}
