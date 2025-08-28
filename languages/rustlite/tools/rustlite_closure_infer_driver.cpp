#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    std::cout << "[rustlite-closure-infer] running...\n";
    // Expect inference to capture the immediately preceding const %cap when no :captures provided.
    Builder b; b.begin_module();
    b.fn_raw("adder", "i32", { {"env","i32"}, {"x","i32"} }, "[ (add %r i32 %env %x) (ret i32 %r) ]");
    b.fn_raw("make_and_call", "i32", {},
        "[ (const %cap i32 7) (rclosure %c adder) (const %arg i32 5) (rcall-closure %res i32 %c %arg) (ret i32 %res) ]");
    b.end_module();
    auto prog=b.build(); auto ast=parse(prog.edn_text); auto expanded=rustlite::expand_rustlite(ast);
    auto s = edn::to_string(expanded);
    bool hasMake = s.find("make-closure")!=std::string::npos;
    bool hasCap = s.find("%cap")!=std::string::npos; // ensure symbol still present
    // After expansion the :captures keyword is consumed; emitted core form is (make-closure %c callee [%cap]) without spaces.
    bool insertedVec = s.find("captures")!=std::string::npos ||
        s.find("make-closure %c adder [%cap]")!=std::string::npos ||
        s.find("make-closure %c adder [ %cap ]")!=std::string::npos ||
        s.find("make-closure %c adder [ %cap]")!=std::string::npos;
    if(!hasMake || !hasCap || !insertedVec){
        std::cerr << "[rustlite-closure-infer] missing expected constructs hasMake="<<hasMake<<" hasCap="<<hasCap<<" inserted="<<insertedVec<<"\n"<<s<<"\n"; return 1; }
    std::cout << "[rustlite-closure-infer] ok\n"; return 0;
}
