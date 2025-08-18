cmake_minimum_required(VERSION 3.20)
project(rustlite LANGUAGES CXX)

add_library(rustlite STATIC
  src/rustlite.cpp
  src/expand.cpp
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

# Logic driver to validate rand/ror semantics and avoid regressions
add_executable(rustlite_logicdriver tools/rustlite_logicdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_logicdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_logicdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.logic COMMAND rustlite_logicdriver)

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

# rdot negative driver
add_executable(rustlite_rdot_negdriver tools/rustlite_rdot_negdriver.cpp)
if(TARGET edn)
  target_link_libraries(rustlite_rdot_negdriver PRIVATE rustlite edn)
endif()
target_compile_features(rustlite_rdot_negdriver PRIVATE cxx_std_20)
add_test(NAME rustlite.rdot_neg COMMAND rustlite_rdot_negdriver)

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
