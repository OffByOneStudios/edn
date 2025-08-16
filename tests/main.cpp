#include <cassert>
#include <iostream>
#include <cassert>
#include <iostream>
#include "edn/edn.hpp"
#include "edn/transform.hpp"
// Forward declaration from types_test.cpp
void run_type_tests();
void run_type_checker_tests();
void run_ir_emitter_test();
void run_cast_tests();
void run_globals_tests();
void run_diagnostics_tests();
void run_phase2_feature_tests();
void run_phase3_pointer_arith_tests();
void run_phase3_addr_deref_tests();
void run_phase3_fnptr_tests();
int run_phase3_typedef_tests();
int run_phase3_enum_tests();
void phase3_for_continue_tests();
void run_phase3_switch_tests();
int run_phase3_union_tests();
int run_phase3_variadic_tests();
int run_phase3_variadic_runtime_test();
int run_phase3_cast_sugar_test();
void run_phase3_diagnostics_notes_tests();
int run_phase3_diagnostics_json_tests();
void run_phase3_examples_smoke();
void run_phase4_sum_types_tests();
void run_phase4_sum_ir_golden_tests();
void run_phase4_sum_resultmode_repro();
void run_phase4_lints_tests();
void run_phase4_generics_macro_test();
void run_phase4_generics_two_params_test();
void run_phase4_generics_dedup_test();
void run_phase4_generics_negative_tests();
void run_phase4_traits_macro_test();

int main(){
    using namespace edn;
    auto v = parse("[1 2 3 :kw true false nil]");
    std::string out = to_string(v);
    // Basic round-trip shape test (not exact due to ordering)
    assert(out.find("[") == 0);
    auto m = parse("{:a 1 :b 2}");
    auto s = to_string(m);
    assert(s.find("{") == 0);
    // Demonstrate macro + visitor:
    // Define a macro 'inc' that rewrites (inc x) -> (+ :lhs x :rhs 1)
    Transformer tr;
    tr.add_macro("inc", [&](const list& form)->std::optional<node_ptr>{
        if(form.elems.size()==2){
            list out;
            out.elems.push_back(std::make_shared<node>(node{symbol{"+"},{}}));
            out.elems.push_back(std::make_shared<node>(node{keyword{"lhs"},{}}));
            out.elems.push_back(form.elems[1]);
            out.elems.push_back(std::make_shared<node>(node{keyword{"rhs"},{}}));
            out.elems.push_back(std::make_shared<node>(node{int64_t{1},{}}));
            return std::make_shared<node>(node{out,{}});
        }
        return std::nullopt;
    });
    int plus_seen = 0;
    tr.add_visitor("+", [&](node& n, list& l, const symbol&){
        // annotate metadata
        n.metadata["kind"] = std::make_shared<node>(node{ keyword{"binary"}, {} });
        ++plus_seen;
    });
    auto form = parse("(inc 41)");
    auto expanded = tr.expand_and_traverse(form);
    assert(plus_seen==1);
    // verify annotation exists
    assert(expanded->data.index()!=0); // not nil


    // Run full test suite
    run_type_tests();
    run_type_checker_tests();
    run_ir_emitter_test();
    run_cast_tests();
    run_globals_tests();
    run_diagnostics_tests();
    run_phase2_feature_tests();
    run_phase3_pointer_arith_tests();
    run_phase3_addr_deref_tests();
    run_phase3_fnptr_tests();
    (void)run_phase3_typedef_tests();
    (void)run_phase3_enum_tests();
    phase3_for_continue_tests();
    run_phase3_switch_tests();
    (void)run_phase3_union_tests();
    (void)run_phase3_variadic_tests();
    (void)run_phase3_variadic_runtime_test();
    (void)run_phase3_cast_sugar_test();
    run_phase3_diagnostics_notes_tests();
    (void)run_phase3_diagnostics_json_tests();
    run_phase3_examples_smoke();
    std::cout << "[dbg] before run_phase4_sum_types_tests" << std::endl;
    run_phase4_sum_types_tests();
    std::cout << "[dbg] after run_phase4_sum_types_tests" << std::endl;
    // Re-enable golden IR tests to verify and localize previously observed segfault
    run_phase4_sum_ir_golden_tests();
    run_phase4_lints_tests();
    run_phase4_generics_macro_test();
    run_phase4_generics_two_params_test();
    run_phase4_generics_dedup_test();
    run_phase4_generics_negative_tests();
    std::cout << "[dbg] before run_phase4_traits_macro_test" << std::endl;
    run_phase4_traits_macro_test();
    std::cout << "[dbg] after run_phase4_traits_macro_test" << std::endl;
    std::cout << "All tests passed" << std::endl;
    return 0;
}
