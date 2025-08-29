// Negative driver placeholder: eventually we want parser-level rejection of keywords-as-identifiers.
// Current placeholder just constructs a trivial module and asserts type checker runs (no specific code yet).
#include <iostream>
#include "rustlite/rustlite.hpp"
#include "edn/edn.hpp"
#include "edn/type_check.hpp"
#include "rustlite/expand.hpp"

int main(){
    using namespace rustlite; using namespace edn;
    Builder b; b.begin_module();
    // Intentionally attempt to use %fn as a variable name to mimic reserved word misuse pattern in macro layer.
    b.fn_raw("kw", "i32", {}, "[ (const %fn i32 1) (ret i32 %fn) ]");
    b.end_module();
    auto prog = b.build(); auto ast = parse(prog.edn_text); auto expanded = rustlite::expand_rustlite(ast);
    TypeContext tctx; TypeChecker tc(tctx); auto res = tc.check_module(expanded);
    bool sawAny = !res.errors.empty(); // may or may not fire; ensure pipeline stability
    std::cout << "[rustlite-keyword-ident-neg] ok" << (sawAny?" (errors observed)":" (no errors)") << "\n"; return 0;
}
