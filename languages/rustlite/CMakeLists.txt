cmake_minimum_required(VERSION 3.20)
project(rustlite LANGUAGES CXX)

add_library(rustlite STATIC
  src/rustlite.cpp
  src/expand.cpp
  src/macros_literals.cpp
  src/macros_control.cpp
  src/macros_sum_enum.cpp
  src/macros_call_logic.cpp
  src/macros_struct_trait.cpp
  src/macros_field_index.cpp
  src/macros_tuple_array.cpp
  src/macros_assert.cpp
  src/macros_alias.cpp
)

target_include_directories(rustlite PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(rustlite PUBLIC edn)

target_compile_features(rustlite PUBLIC cxx_std_20)

enable_testing()
add_executable(rustlite_driver tools/rustlite_driver.cpp)

# Link language driver to edn + rustlite
if(TARGET edn)
  target_link_libraries(rustlite_driver PRIVATE rustlite edn)
endif()

target_compile_features(rustlite_driver PRIVATE cxx_std_20)

add_test(NAME rustlite.smoke COMMAND rustlite_driver)

# Minimal standalone test driver for field/index sugar
add_executable(rustlite_minidriver tools/rustlite_minidriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_minidriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_minidriver PRIVATE cxx_std_20)
add_test(NAME rustlite.fields_index COMMAND rustlite_minidriver)
set_tests_properties(rustlite.fields_index PROPERTIES DISABLED FALSE)

add_executable(rustlite_negdriver tools/rustlite_negdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_negdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.negatives COMMAND rustlite_negdriver)

# Trait-object IR shape demo
add_executable(rustlite_traitdriver tools/rustlite_traitdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_traitdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_traitdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.traits_shape COMMAND rustlite_traitdriver)

# Generics demo (monomorphization via duplicated defs)
add_executable(rustlite_genericsdriver tools/rustlite_genericsdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_genericsdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_genericsdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.generics COMMAND rustlite_genericsdriver)
set_tests_properties(rustlite.generics PROPERTIES DISABLED TRUE)

# Extern C call smoke
add_executable(rustlite_externdriver tools/rustlite_externdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_externdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_externdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.extern_c COMMAND rustlite_externdriver)

# Panic toggle driver (runs twice with different env)
add_executable(rustlite_panicdriver tools/rustlite_panicdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_panicdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_panicdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.panic_abort COMMAND rustlite_panicdriver)
set_tests_properties(rustlite.panic_abort PROPERTIES ENVIRONMENT "EDN_ENABLE_EH=0;EDN_PANIC=abort")
add_test(NAME rustlite.panic_unwind COMMAND rustlite_panicdriver)
set_tests_properties(rustlite.panic_unwind PROPERTIES ENVIRONMENT "EDN_ENABLE_EH=1;EDN_PANIC=unwind")
set_tests_properties(rustlite.panic_unwind PROPERTIES DISABLED TRUE)

# Logic driver to validate rand/ror semantics and avoid regressions
add_executable(rustlite_logicdriver tools/rustlite_logicdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_logicdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_logicdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.logic COMMAND rustlite_logicdriver)

# rfor driver (macro coverage for for-loop lowering)
add_executable(rustlite_rfor_driver tools/rustlite_rfor_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_rfor_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_rfor_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rfor COMMAND rustlite_rfor_driver)

# struct fields dedicated driver
add_executable(rustlite_struct_fields_driver tools/rustlite_struct_fields_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_struct_fields_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_struct_fields_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.struct_fields COMMAND rustlite_struct_fields_driver)

# indexing dedicated driver
add_executable(rustlite_indexing_driver tools/rustlite_indexing_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_indexing_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_indexing_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.indexing COMMAND rustlite_indexing_driver)

# tuple/array (Phase A) smoke driver
add_executable(rustlite_tuple_array_driver tools/rustlite_tuple_array_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_tuple_array_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_tuple_array_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.tuple_array COMMAND rustlite_tuple_array_driver)

# tuple basic driver
add_executable(rustlite_tuple_basic_driver tools/rustlite_tuple_basic_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_tuple_basic_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_tuple_basic_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.tuple_basic COMMAND rustlite_tuple_basic_driver)

# closure positive driver
add_executable(rustlite_closure_driver tools/rustlite_closure_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_closure_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_closure_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.closure COMMAND rustlite_closure_driver)

# closure negative driver
add_executable(rustlite_closure_negdriver tools/rustlite_closure_negdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_closure_negdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_closure_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.closure_neg COMMAND rustlite_closure_negdriver)
set_tests_properties(rustlite.closure_neg PROPERTIES LABELS "rustlite-neg")

# option/result dedicated driver
add_executable(rustlite_option_result_driver tools/rustlite_option_result_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_option_result_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_option_result_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.option_result COMMAND rustlite_option_result_driver)

# fnptr driver
add_executable(rustlite_fnptr_driver tools/rustlite_fnptr_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_fnptr_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_fnptr_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.fnptr COMMAND rustlite_fnptr_driver)

# trait negative arity driver
add_executable(rustlite_trait_negdriver tools/rustlite_trait_negdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_trait_negdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_trait_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.trait_neg COMMAND rustlite_trait_negdriver)
set_tests_properties(rustlite.trait_neg PROPERTIES LABELS "rustlite-neg")

# extern-global missing init negative (const extern without :init)
add_executable(rustlite_extern_global_init_negdriver tools/rustlite_extern_global_init_negdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_extern_global_init_negdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_extern_global_init_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.extern_global_init_neg COMMAND rustlite_extern_global_init_negdriver)
set_tests_properties(rustlite.extern_global_init_neg PROPERTIES LABELS "rustlite-neg")

# rwhile-let driver
add_executable(rustlite_rwhilelet_driver tools/rustlite_rwhilelet_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_rwhilelet_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_rwhilelet_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rwhilelet COMMAND rustlite_rwhilelet_driver)

# rdot driver
add_executable(rustlite_rdot_driver tools/rustlite_rdot_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_rdot_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_rdot_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rdot COMMAND rustlite_rdot_driver)

# rtry driver
add_executable(rustlite_rtry_driver tools/rustlite_rtry_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_rtry_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_rtry_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rtry_result COMMAND rustlite_rtry_driver)

# rtry option driver
add_executable(rustlite_rtry_option_driver tools/rustlite_rtry_option_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_rtry_option_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_rtry_option_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rtry_option COMMAND rustlite_rtry_option_driver)

add_executable(rustlite_rtry_negdriver tools/rustlite_rtry_negdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_rtry_negdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_rtry_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.rtry_negative COMMAND rustlite_rtry_negdriver)
# rdot negative driver
add_executable(rustlite_rdot_negdriver tools/rustlite_rdot_negdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_rdot_negdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_rdot_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.rdot_neg COMMAND rustlite_rdot_negdriver)
set_tests_properties(rustlite.rdot_neg PROPERTIES LABELS "rustlite-neg")

# renum + rmatch demo driver
add_executable(rustlite_enum_driver tools/rustlite_enum_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_enum_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_enum_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.enum COMMAND rustlite_enum_driver)

# enum surface macro driver (Phase B start)
add_executable(rustlite_enum_surface_driver tools/rustlite_enum_surface_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_enum_surface_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_enum_surface_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.enum_surface COMMAND rustlite_enum_surface_driver)

# ematch exhaustive driver
add_executable(rustlite_ematch_exhaustive_driver tools/rustlite_ematch_exhaustive_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_ematch_exhaustive_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_ematch_exhaustive_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.ematch_exhaustive COMMAND rustlite_ematch_exhaustive_driver)

# ematch non-exhaustive driver
add_executable(rustlite_ematch_non_exhaustive_driver tools/rustlite_ematch_non_exhaustive_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_ematch_non_exhaustive_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_ematch_non_exhaustive_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.ematch_non_exhaustive COMMAND rustlite_ematch_non_exhaustive_driver)

# ematch payload-binding driver
add_executable(rustlite_ematch_payload_driver tools/rustlite_ematch_payload_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_ematch_payload_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_ematch_payload_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.ematch_payload COMMAND rustlite_ematch_payload_driver)


# legacy non-exhaustive rmatch regression driver
add_executable(rustlite_rmatch_non_exhaustive_legacy_driver tools/rustlite_rmatch_non_exhaustive_legacy_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_rmatch_non_exhaustive_legacy_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_rmatch_non_exhaustive_legacy_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rmatch_non_exhaustive_legacy COMMAND rustlite_rmatch_non_exhaustive_legacy_driver)

# rmake-trait-obj focused driver
add_executable(rustlite_make_trait_obj_driver tools/rustlite_make_trait_obj_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_make_trait_obj_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_make_trait_obj_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.make_trait_obj COMMAND rustlite_make_trait_obj_driver)

# rcstr unknown escape permissive behavior driver
add_executable(rustlite_cstr_unknown_escape_driver tools/rustlite_cstr_unknown_escape_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_cstr_unknown_escape_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_cstr_unknown_escape_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.cstr_unknown_escape COMMAND rustlite_cstr_unknown_escape_driver)


# Parser library (OBJECT for now to keep compile scoping simple)
add_library(rustlite_parser OBJECT
  parser/parser.cpp
)
target_include_directories(rustlite_parser PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/parser>
)
target_compile_features(rustlite_parser PUBLIC cxx_std_20)

# CLI: rustlitec
add_executable(rustlitec tools/rustlitec.cpp $<TARGET_OBJECTS:rustlite_parser>)
target_link_libraries(rustlitec PRIVATE rustlite edn)
target_compile_features(rustlitec PRIVATE cxx_std_20)

# Parser smoke test
add_executable(rustlite_parser_smoke tools/rustlite_parser_smoke.cpp $<TARGET_OBJECTS:rustlite_parser>)
target_link_libraries(rustlite_parser_smoke PRIVATE rustlite edn)
target_compile_features(rustlite_parser_smoke PRIVATE cxx_std_20)
add_test(NAME rustlite.parser_smoke COMMAND rustlite_parser_smoke)

# CLI smoke: run rustlitec on a tiny sample
add_test(NAME rustlite.cli_smoke COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/empty_main.rl.rs)
add_test(NAME rustlite.cli_return COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/return.rl.rs)
add_test(NAME rustlite.cli_let COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/let_stmt.rl.rs)
add_test(NAME rustlite.cli_let_typed COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/let_typed.rl.rs)
add_test(NAME rustlite.cli_unary_minus COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/unary_minus.rl.rs)
add_test(NAME rustlite.cli_ident_stmt COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/ident_stmt.rl.rs)
add_test(NAME rustlite.cli_empty_call COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/empty_call.rl.rs)
add_test(NAME rustlite.cli_call_args COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/call_args.rl.rs)
add_test(NAME rustlite.cli_break_continue COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/break_continue.rl.rs)
add_test(NAME rustlite.cli_if_stmt COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/if_stmt.rl.rs)
add_test(NAME rustlite.cli_if_else COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/if_else.rl.rs)
add_test(NAME rustlite.cli_while_stmt COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/while_stmt.rl.rs)
add_test(NAME rustlite.cli_loop_stmt COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/loop_stmt.rl.rs)
add_test(NAME rustlite.cli_else_if_chain COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/else_if_chain.rl.rs)
add_test(NAME rustlite.cli_nested_blocks COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/nested_blocks.rl.rs)
add_test(NAME rustlite.cli_assign_stmt COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/assign_stmt.rl.rs)
add_test(NAME rustlite.cli_paren_exprs COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/paren_exprs.rl.rs)
add_test(NAME rustlite.cli_logical_ops COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/logical_ops.rl.rs)
add_test(NAME rustlite.cli_unary_not COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/unary_not.rl.rs)
add_test(NAME rustlite.cli_mixed_precedence COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/mixed_precedence.rl.rs)
add_test(NAME rustlite.cli_fn_params COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/fn_params.rl.rs)
add_test(NAME rustlite.cli_compound_assign COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/compound_assign.rl.rs)
add_test(NAME rustlite.cli_let_mut COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/let_mut.rl.rs)
add_test(NAME rustlite.cli_complex_lowering COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/complex_lowering.rl.rs)
add_test(NAME rustlite.cli_enum_basic COMMAND rustlitec ${CMAKE_CURRENT_SOURCE_DIR}/samples/enum_basic.rl.rs)

# End-to-end driver: parse Rustlite source -> EDN -> expand -> typecheck -> emit IR
add_executable(rustlite_e2e_driver tools/rustlite_e2e_driver.cpp $<TARGET_OBJECTS:rustlite_parser>)
if(TARGET edn)
  target_link_libraries(rustlite_e2e_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_e2e_driver PRIVATE cxx_std_20)

# A few representative E2E samples
add_test(NAME rustlite.e2e_return COMMAND rustlite_e2e_driver ${CMAKE_CURRENT_SOURCE_DIR}/samples/return.rl.rs)
add_test(NAME rustlite.e2e_logical_ops COMMAND rustlite_e2e_driver ${CMAKE_CURRENT_SOURCE_DIR}/samples/logical_ops_e2e.rl.rs)
add_test(NAME rustlite.e2e_fn_params COMMAND rustlite_e2e_driver ${CMAKE_CURRENT_SOURCE_DIR}/samples/fn_params.rl.rs)
add_test(NAME rustlite.e2e_complex_lowering COMMAND rustlite_e2e_driver ${CMAKE_CURRENT_SOURCE_DIR}/samples/complex_lowering.rl.rs --dump)

# JIT driver to execute Rustlite via LLVM ORC and capture a result
add_executable(rustlite_jit_driver tools/rustlite_jit_driver.cpp $<TARGET_OBJECTS:rustlite_parser>)
if(TARGET edn)
  target_link_libraries(rustlite_jit_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_jit_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.jit_return COMMAND rustlite_jit_driver ${CMAKE_CURRENT_SOURCE_DIR}/samples/return.rl.rs)
set_tests_properties(rustlite.jit_return PROPERTIES PASS_REGULAR_EXPRESSION "Result: 42")

# Debug variant: verify --debug prints source, frontend EDN, lowered core EDN, and result
add_test(NAME rustlite.jit_return_debug COMMAND rustlite_jit_driver ${CMAKE_CURRENT_SOURCE_DIR}/samples/return.rl.rs --debug)
set_tests_properties(rustlite.jit_return_debug PROPERTIES PASS_REGULAR_EXPRESSION "=== Rust Source ===.*=== Frontend EDN ===.*=== Lowered Core EDN ===.*=== JIT Result ===.*Result: 42")

add_test(NAME rustlite.jit_complex_debug COMMAND rustlite_jit_driver ${CMAKE_CURRENT_SOURCE_DIR}/samples/complex_lowering.rl.rs --debug)
set_tests_properties(rustlite.jit_complex_debug PROPERTIES PASS_REGULAR_EXPRESSION "=== Rust Source ===.*=== Frontend EDN ===.*=== Lowered Core EDN ===.*=== JIT Result ===")
set_tests_properties(rustlite.jit_complex_debug PROPERTIES DISABLED TRUE)

# literals demo driver
add_executable(rustlite_literals_driver tools/rustlite_literals_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_literals_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_literals_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.literals COMMAND rustlite_literals_driver)

# extern globals + literal interning reuse driver
add_executable(rustlite_extern_globals_driver tools/rustlite_extern_globals_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_extern_globals_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_extern_globals_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.extern_globals COMMAND rustlite_extern_globals_driver)

# extern globals negative driver
add_executable(rustlite_extern_globals_negdriver tools/rustlite_extern_globals_negdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_extern_globals_negdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_extern_globals_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.extern_globals_neg COMMAND rustlite_extern_globals_negdriver)
set_tests_properties(rustlite.extern_globals_neg PROPERTIES LABELS "rustlite-neg")

# literals alias/interning audit
add_executable(rustlite_literals_alias_audit_driver tools/rustlite_literals_alias_audit_driver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_literals_alias_audit_driver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_literals_alias_audit_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.literals_alias_audit COMMAND rustlite_literals_alias_audit_driver)

# literals negative driver
add_executable(rustlite_literals_negdriver tools/rustlite_literals_negdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_literals_negdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_literals_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.literals_neg COMMAND rustlite_literals_negdriver)
set_tests_properties(rustlite.literals_neg PROPERTIES LABELS "rustlite-neg")

# rustlite_index_addr_driver driver
add_executable(rustlite_index_addr_driver tools/rustlite_index_addr_driver.cpp)
  target_link_libraries(rustlite_index_addr_driver PRIVATE rustlite edn)
target_compile_features(rustlite_index_addr_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.index_addr COMMAND rustlite_index_addr_driver)

# rustlite_bounds_driver (bounds checking demonstration)
add_executable(rustlite_bounds_driver tools/rustlite_bounds_driver.cpp)
  target_link_libraries(rustlite_bounds_driver PRIVATE rustlite edn)
target_compile_features(rustlite_bounds_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.bounds_expand COMMAND rustlite_bounds_driver)

# Bounds OK / OOB focused drivers
add_executable(rustlite_index_bounds_ok_driver tools/rustlite_index_bounds_ok_driver.cpp)
  target_link_libraries(rustlite_index_bounds_ok_driver PRIVATE rustlite edn)
target_compile_features(rustlite_index_bounds_ok_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.index_bounds_ok COMMAND rustlite_index_bounds_ok_driver)

add_executable(rustlite_index_bounds_oob_driver tools/rustlite_index_bounds_oob_driver.cpp)
  target_link_libraries(rustlite_index_bounds_oob_driver PRIVATE rustlite edn)
target_compile_features(rustlite_index_bounds_oob_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.index_bounds_oob_expand COMMAND rustlite_index_bounds_oob_driver)

# Phase D operator support test driver
add_executable(rustlite_bitwise_ops_driver tools/rustlite_bitwise_ops_driver.cpp)
target_link_libraries(rustlite_bitwise_ops_driver PRIVATE rustlite edn)
target_compile_features(rustlite_bitwise_ops_driver PRIVATE cxx_std_20)
target_include_directories(rustlite_bitwise_ops_driver PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME rustlite.bitwise_ops COMMAND rustlite_bitwise_ops_driver)

# Compound assignment driver
add_executable(rustlite_compound_assign_driver tools/rustlite_compound_assign_driver.cpp)
target_link_libraries(rustlite_compound_assign_driver PRIVATE rustlite edn)
target_compile_features(rustlite_compound_assign_driver PRIVATE cxx_std_20)
target_include_directories(rustlite_compound_assign_driver PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME rustlite.compound_assign COMMAND rustlite_compound_assign_driver)

# Range for driver
add_executable(rustlite_range_for_driver tools/rustlite_range_for_driver.cpp)
target_link_libraries(rustlite_range_for_driver PRIVATE rustlite edn)
target_compile_features(rustlite_range_for_driver PRIVATE cxx_std_20)
target_include_directories(rustlite_range_for_driver PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME rustlite.range_for COMMAND rustlite_range_for_driver)

# Range literal driver
add_executable(rustlite_range_literal_driver tools/rustlite_range_literal_driver.cpp)
target_link_libraries(rustlite_range_literal_driver PRIVATE rustlite edn)
target_compile_features(rustlite_range_literal_driver PRIVATE cxx_std_20)
target_include_directories(rustlite_range_literal_driver PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME rustlite.range_literal COMMAND rustlite_range_literal_driver)

add_executable(rustlite_range_tuple_inclusive_driver tools/rustlite_range_tuple_inclusive_driver.cpp)
  target_link_libraries(rustlite_range_tuple_inclusive_driver PRIVATE rustlite edn)
  target_compile_features(rustlite_range_tuple_inclusive_driver PRIVATE cxx_std_20)
  target_include_directories(rustlite_range_tuple_inclusive_driver PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME rustlite.range_tuple_inclusive COMMAND rustlite_range_tuple_inclusive_driver)

# Range inclusive literal driver
add_executable(rustlite_range_inclusive_driver tools/rustlite_range_inclusive_driver.cpp)
target_link_libraries(rustlite_range_inclusive_driver PRIVATE rustlite edn)
target_compile_features(rustlite_range_inclusive_driver PRIVATE cxx_std_20)
target_include_directories(rustlite_range_inclusive_driver PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME rustlite.range_inclusive COMMAND rustlite_range_inclusive_driver)

# Range syntax negative driver (malformed rfor-range invocations)
add_executable(rustlite_range_negdriver tools/rustlite_range_negdriver.cpp)
target_link_libraries(rustlite_range_negdriver PRIVATE rustlite edn)
target_compile_features(rustlite_range_negdriver PRIVATE cxx_std_20)
target_include_directories(rustlite_range_negdriver PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_test(NAME rustlite.range_neg COMMAND rustlite_range_negdriver)
set_tests_properties(rustlite.range_neg PROPERTIES LABELS "rustlite-neg")

# Keyword-as-identifier negative driver
add_executable(rustlite_keyword_ident_negdriver tools/rustlite_keyword_ident_negdriver.cpp)
target_link_libraries(rustlite_keyword_ident_negdriver PRIVATE rustlite edn)
target_compile_features(rustlite_keyword_ident_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.keyword_ident_neg COMMAND rustlite_keyword_ident_negdriver)
set_tests_properties(rustlite.keyword_ident_neg PROPERTIES LABELS "rustlite-neg")

# Tuple destructuring placeholder driver
add_executable(rustlite_tuple_destructure_driver tools/rustlite_tuple_destructure_driver.cpp)
target_link_libraries(rustlite_tuple_destructure_driver PRIVATE rustlite edn)
target_compile_features(rustlite_tuple_destructure_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.tuple_destructure COMMAND rustlite_tuple_destructure_driver)

# Tuple destructure negative tests (arity mismatch, non-tuple target)
add_executable(rustlite_tuple_destructure_arity_mismatch_negdriver tools/rustlite_tuple_destructure_arity_mismatch_negdriver.cpp)
  target_link_libraries(rustlite_tuple_destructure_arity_mismatch_negdriver PRIVATE rustlite edn)
  target_compile_features(rustlite_tuple_destructure_arity_mismatch_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.tuple_destructure_arity_mismatch_neg COMMAND rustlite_tuple_destructure_arity_mismatch_negdriver)

add_executable(rustlite_tuple_destructure_non_tuple_negdriver tools/rustlite_tuple_destructure_non_tuple_negdriver.cpp)
  target_link_libraries(rustlite_tuple_destructure_non_tuple_negdriver PRIVATE rustlite edn)
  target_compile_features(rustlite_tuple_destructure_non_tuple_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.tuple_destructure_non_tuple_neg COMMAND rustlite_tuple_destructure_non_tuple_negdriver)

# Tuple destructure positive test (no diagnostics expected)
add_executable(rustlite_tuple_destructure_positive_driver tools/rustlite_tuple_destructure_positive_driver.cpp)
  target_link_libraries(rustlite_tuple_destructure_positive_driver PRIVATE rustlite edn)
  target_compile_features(rustlite_tuple_destructure_positive_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.tuple_destructure_positive COMMAND rustlite_tuple_destructure_positive_driver)

add_executable(rustlite_tuple_destructure_over_arity_negdriver tools/rustlite_tuple_destructure_over_arity_negdriver.cpp)
  target_link_libraries(rustlite_tuple_destructure_over_arity_negdriver PRIVATE rustlite edn)
  target_compile_features(rustlite_tuple_destructure_over_arity_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.tuple_destructure_over_arity_neg COMMAND rustlite_tuple_destructure_over_arity_negdriver)

add_executable(rustlite_tuple_destructure_gapped_driver tools/rustlite_tuple_destructure_gapped_driver.cpp)
  target_link_libraries(rustlite_tuple_destructure_gapped_driver PRIVATE rustlite edn)
  target_compile_features(rustlite_tuple_destructure_gapped_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.tuple_destructure_gapped COMMAND rustlite_tuple_destructure_gapped_driver)

# Match tuple pattern positive (Phase 2a initial)
add_executable(rustlite_match_tuple_positive_driver tools/rustlite_match_tuple_positive_driver.cpp)
  target_link_libraries(rustlite_match_tuple_positive_driver PRIVATE rustlite edn)
  target_compile_features(rustlite_match_tuple_positive_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.match_tuple_positive COMMAND rustlite_match_tuple_positive_driver)

# Match tuple pattern negative: arity mismatch
add_executable(rustlite_match_tuple_arity_mismatch_negdriver tools/rustlite_match_tuple_arity_mismatch_negdriver.cpp)
  target_link_libraries(rustlite_match_tuple_arity_mismatch_negdriver PRIVATE rustlite edn)
  target_compile_features(rustlite_match_tuple_arity_mismatch_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.match_tuple_arity_mismatch_neg COMMAND rustlite_match_tuple_arity_mismatch_negdriver)

# Match tuple pattern negative: non-tuple target
add_executable(rustlite_match_tuple_non_tuple_negdriver tools/rustlite_match_tuple_non_tuple_negdriver.cpp)
  target_link_libraries(rustlite_match_tuple_non_tuple_negdriver PRIVATE rustlite edn)
  target_compile_features(rustlite_match_tuple_non_tuple_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.match_tuple_non_tuple_neg COMMAND rustlite_match_tuple_non_tuple_negdriver)

# Match tuple pattern negative: over arity (extra tget beyond tuple size)
add_executable(rustlite_match_tuple_over_arity_negdriver tools/rustlite_match_tuple_over_arity_negdriver.cpp)
  target_link_libraries(rustlite_match_tuple_over_arity_negdriver PRIVATE rustlite edn)
  target_compile_features(rustlite_match_tuple_over_arity_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.match_tuple_over_arity_neg COMMAND rustlite_match_tuple_over_arity_negdriver)

# Match tuple pattern gapped (no diagnostics expected)
add_executable(rustlite_match_tuple_gapped_driver tools/rustlite_match_tuple_gapped_driver.cpp)
  target_link_libraries(rustlite_match_tuple_gapped_driver PRIVATE rustlite edn)
  target_compile_features(rustlite_match_tuple_gapped_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.match_tuple_gapped COMMAND rustlite_match_tuple_gapped_driver)

# if-let / while-let driver
add_executable(rustlite_if_while_let_driver tools/rustlite_if_while_let_driver.cpp)
target_link_libraries(rustlite_if_while_let_driver PRIVATE rustlite edn)
target_compile_features(rustlite_if_while_let_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.if_while_let COMMAND rustlite_if_while_let_driver)

# Negative: missing :value in rif-let branch
add_executable(rustlite_if_while_let_neg_missing_value_driver tools/rustlite_if_while_let_neg_missing_value_driver.cpp)
target_link_libraries(rustlite_if_while_let_neg_missing_value_driver PRIVATE rustlite edn)
target_compile_features(rustlite_if_while_let_neg_missing_value_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.if_while_let.neg.missing_value COMMAND rustlite_if_while_let_neg_missing_value_driver)

# Negative: missing :value in rif-let default branch
add_executable(rustlite_if_while_let_neg_missing_default_value_driver tools/rustlite_if_while_let_neg_missing_default_value_driver.cpp)
target_link_libraries(rustlite_if_while_let_neg_missing_default_value_driver PRIVATE rustlite edn)
target_compile_features(rustlite_if_while_let_neg_missing_default_value_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.if_while_let.neg.missing_default_value COMMAND rustlite_if_while_let_neg_missing_default_value_driver)
set_tests_properties(rustlite.if_while_let.neg.missing_default_value PROPERTIES LABELS "rustlite-neg")

# Negative: rif-let case result type mismatch
add_executable(rustlite_if_while_let_neg_case_result_type_mismatch_driver tools/rustlite_if_while_let_neg_case_result_type_mismatch_driver.cpp)
target_link_libraries(rustlite_if_while_let_neg_case_result_type_mismatch_driver PRIVATE rustlite edn)
target_compile_features(rustlite_if_while_let_neg_case_result_type_mismatch_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.if_while_let.neg.case_result_type_mismatch COMMAND rustlite_if_while_let_neg_case_result_type_mismatch_driver)
set_tests_properties(rustlite.if_while_let.neg.case_result_type_mismatch PROPERTIES LABELS "rustlite-neg")

# Negative: rif-let default result type mismatch
add_executable(rustlite_if_while_let_neg_default_result_type_mismatch_driver tools/rustlite_if_while_let_neg_default_result_type_mismatch_driver.cpp)
target_link_libraries(rustlite_if_while_let_neg_default_result_type_mismatch_driver PRIVATE rustlite edn)
target_compile_features(rustlite_if_while_let_neg_default_result_type_mismatch_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.if_while_let.neg.default_result_type_mismatch COMMAND rustlite_if_while_let_neg_default_result_type_mismatch_driver)
set_tests_properties(rustlite.if_while_let.neg.default_result_type_mismatch PROPERTIES LABELS "rustlite-neg")

# rloop-val positive driver
add_executable(rustlite_rloop_val_driver tools/rustlite_rloop_val_driver.cpp)
target_link_libraries(rustlite_rloop_val_driver PRIVATE rustlite edn)
target_compile_features(rustlite_rloop_val_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rloop_val COMMAND rustlite_rloop_val_driver)

# New: rloop-val plain break (no value) positive driver
add_executable(rustlite_rloop_val_plain_break_driver tools/rustlite_rloop_val_plain_break_driver.cpp)
target_link_libraries(rustlite_rloop_val_plain_break_driver PRIVATE rustlite edn)
target_compile_features(rustlite_rloop_val_plain_break_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rloop_val.plain_break COMMAND rustlite_rloop_val_plain_break_driver)

# New: rloop-val no-break positive (compilation only)
add_executable(rustlite_rloop_val_no_break_driver tools/rustlite_rloop_val_no_break_driver.cpp)
target_link_libraries(rustlite_rloop_val_no_break_driver PRIVATE rustlite edn)
target_compile_features(rustlite_rloop_val_no_break_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rloop_val.no_break COMMAND rustlite_rloop_val_no_break_driver)

# New: rloop-val multi-break (same type) positive
add_executable(rustlite_rloop_val_multi_break_driver tools/rustlite_rloop_val_multi_break_driver.cpp)
target_link_libraries(rustlite_rloop_val_multi_break_driver PRIVATE rustlite edn)
target_compile_features(rustlite_rloop_val_multi_break_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.rloop_val.multi_break COMMAND rustlite_rloop_val_multi_break_driver)

# Closure capture inference (enabled)
add_executable(rustlite_closure_infer_driver tools/rustlite_closure_infer_driver.cpp)
target_link_libraries(rustlite_closure_infer_driver PRIVATE rustlite edn)
target_compile_features(rustlite_closure_infer_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.closure_infer COMMAND rustlite_closure_infer_driver)
set_tests_properties(rustlite.closure_infer PROPERTIES ENVIRONMENT "RUSTLITE_INFER_CAPS=1")

# Closure capture inference (disabled)
add_executable(rustlite_closure_infer_disabled_driver tools/rustlite_closure_infer_disabled_driver.cpp)
target_link_libraries(rustlite_closure_infer_disabled_driver PRIVATE rustlite edn)
target_compile_features(rustlite_closure_infer_disabled_driver PRIVATE cxx_std_20)
add_test(NAME rustlite.closure_infer_disabled COMMAND rustlite_closure_infer_disabled_driver)


# Surface tests (parser golden comparisons)
add_subdirectory(surface_tests)

# Register new rif-let variant mismatch negative driver
add_executable(rustlite_if_while_let_neg_variant_mismatch_driver tools/rustlite_if_while_let_neg_variant_mismatch_driver.cpp)
target_link_libraries(rustlite_if_while_let_neg_variant_mismatch_driver PRIVATE rustlite edn)
add_test(NAME rustlite.if_while_let.neg.variant_mismatch COMMAND rustlite_if_while_let_neg_variant_mismatch_driver)

# Register new rif-let undefined :value negative driver
add_executable(rustlite_if_while_let_neg_undefined_value_driver tools/rustlite_if_while_let_neg_undefined_value_driver.cpp)
target_link_libraries(rustlite_if_while_let_neg_undefined_value_driver PRIVATE rustlite edn)
add_test(NAME rustlite.if_while_let.neg.undefined_value COMMAND rustlite_if_while_let_neg_undefined_value_driver)

# Type alias driver
add_executable(rustlite_type_alias_driver tools/rustlite_type_alias_driver.cpp)
target_link_libraries(rustlite_type_alias_driver PRIVATE rustlite edn)
add_test(NAME rustlite.type_alias COMMAND rustlite_type_alias_driver)

# Type alias negative tests
add_executable(rustlite_type_alias_neg_missing_name_driver tools/rustlite_type_alias_neg_missing_name_driver.cpp)
target_link_libraries(rustlite_type_alias_neg_missing_name_driver PRIVATE rustlite edn)
add_test(NAME rustlite.type_alias.neg.missing_name COMMAND rustlite_type_alias_neg_missing_name_driver)

add_executable(rustlite_type_alias_neg_missing_type_driver tools/rustlite_type_alias_neg_missing_type_driver.cpp)
target_link_libraries(rustlite_type_alias_neg_missing_type_driver PRIVATE rustlite edn)
add_test(NAME rustlite.type_alias.neg.missing_type COMMAND rustlite_type_alias_neg_missing_type_driver)

add_executable(rustlite_type_alias_neg_redefinition_driver tools/rustlite_type_alias_neg_redefinition_driver.cpp)
target_link_libraries(rustlite_type_alias_neg_redefinition_driver PRIVATE rustlite edn)
add_test(NAME rustlite.type_alias.neg.redefinition COMMAND rustlite_type_alias_neg_redefinition_driver)

add_executable(rustlite_type_alias_neg_invalid_underlying_driver tools/rustlite_type_alias_neg_invalid_underlying_driver.cpp)
target_link_libraries(rustlite_type_alias_neg_invalid_underlying_driver PRIVATE rustlite edn)
add_test(NAME rustlite.type_alias.neg.invalid_underlying COMMAND rustlite_type_alias_neg_invalid_underlying_driver)

add_executable(rustlite_type_alias_neg_shadow_driver tools/rustlite_type_alias_neg_shadow_driver.cpp)
target_link_libraries(rustlite_type_alias_neg_shadow_driver PRIVATE rustlite edn)
add_test(NAME rustlite.type_alias.neg.shadow COMMAND rustlite_type_alias_neg_shadow_driver)

add_executable(rustlite_type_alias_neg_cycle_driver tools/rustlite_type_alias_neg_cycle_driver.cpp)
target_link_libraries(rustlite_type_alias_neg_cycle_driver PRIVATE rustlite edn)
add_test(NAME rustlite.type_alias.neg.cycle COMMAND rustlite_type_alias_neg_cycle_driver)

# Generics basic prototype driver
add_executable(rustlite_generics_basic_driver tools/rustlite_generics_basic_driver.cpp)
target_link_libraries(rustlite_generics_basic_driver PRIVATE rustlite edn)
add_test(NAME rustlite.generics.basic COMMAND rustlite_generics_basic_driver)

# Generic parameter driver (uses :generics + rcall-g expansion)
add_executable(rustlite_generics_param_driver tools/rustlite_generics_param_driver.cpp)
target_link_libraries(rustlite_generics_param_driver PRIVATE rustlite edn)
add_test(NAME rustlite.generics.param COMMAND rustlite_generics_param_driver)

add_executable(rustlite_generics_neg_arity_driver tools/rustlite_generics_neg_arity_driver.cpp)
target_link_libraries(rustlite_generics_neg_arity_driver PRIVATE rustlite edn)
add_test(NAME rustlite.generics.neg.arity COMMAND rustlite_generics_neg_arity_driver)

add_executable(rustlite_generics_neg_unknown_driver tools/rustlite_generics_neg_unknown_driver.cpp)
target_link_libraries(rustlite_generics_neg_unknown_driver PRIVATE rustlite edn)
add_test(NAME rustlite.generics.neg.unknown COMMAND rustlite_generics_neg_unknown_driver)

add_executable(rustlite_generics_bounds_driver tools/rustlite_generics_bounds_driver.cpp)
target_link_libraries(rustlite_generics_bounds_driver PRIVATE rustlite edn)
add_test(NAME rustlite.generics.bounds COMMAND rustlite_generics_bounds_driver)

add_executable(rustlite_generics_neg_bound_driver tools/rustlite_generics_neg_bound_driver.cpp)
target_link_libraries(rustlite_generics_neg_bound_driver PRIVATE rustlite edn)
add_test(NAME rustlite.generics.neg.bound COMMAND rustlite_generics_neg_bound_driver)

# IR specialization verification test: ensures monomorphization produces concrete fn symbols
# tests/ lives at repo root; from languages/rustlite we need to go up two levels.
add_executable(rustlite_generics_specialization_ir_test ../../tests/rustlite_generics_specialization_ir_test.cpp)
target_link_libraries(rustlite_generics_specialization_ir_test PRIVATE rustlite edn)
add_test(NAME rustlite.generics.specialization_ir COMMAND rustlite_generics_specialization_ir_test)

# Tuple match literal guard IR test (standalone to avoid duplicate test mains)
add_executable(rustlite_tuple_match_literal_guard_test ../../tests/rustlite_tuple_match_literal_guard_test.cpp $<TARGET_OBJECTS:rustlite_parser>)
target_link_libraries(rustlite_tuple_match_literal_guard_test PRIVATE rustlite edn)
add_test(NAME rustlite.tuple_match.literal_guard COMMAND rustlite_tuple_match_literal_guard_test)

# rwhile-let parity negative tests
add_executable(rustlite_rwhile_let_neg_variant_mismatch_driver tools/rustlite_rwhile_let_neg_variant_mismatch_driver.cpp)
target_link_libraries(rustlite_rwhile_let_neg_variant_mismatch_driver PRIVATE rustlite edn)
add_test(NAME rustlite.rwhile_let.neg.variant_mismatch COMMAND rustlite_rwhile_let_neg_variant_mismatch_driver)

add_executable(rustlite_rwhile_let_neg_undefined_value_driver tools/rustlite_rwhile_let_neg_undefined_value_driver.cpp)
target_link_libraries(rustlite_rwhile_let_neg_undefined_value_driver PRIVATE rustlite edn)
add_test(NAME rustlite.rwhile_let.neg.undefined_value COMMAND rustlite_rwhile_let_neg_undefined_value_driver)

# Register new variable pattern tuple match test
add_executable(rustlite_tuple_match_variable_pattern_test ../../tests/rustlite_tuple_match_variable_pattern_test.cpp $<TARGET_OBJECTS:rustlite_parser>)
target_link_libraries(rustlite_tuple_match_variable_pattern_test PRIVATE rustlite edn)
add_test(NAME rustlite.tuple_match.variable_pattern COMMAND rustlite_tuple_match_variable_pattern_test)

